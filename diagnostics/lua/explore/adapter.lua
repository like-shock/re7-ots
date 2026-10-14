-- Exploration camera with opt-in RT models and experimental obstacle avoidance.
local VERSION='0.5.0-dev'
local DIR='RE7_OTS_Explore/'
local gate=Core.new()
local head_session=ModelGroup.new(sdk)
local physics=Physics.new(sdk)
local obstacle=Obstacle.new()
local collision_report={status='DISABLED'}
local cfg=Core.settings(nil)
local requested=false -- Never restore an enabled state from disk.
local fault,callback_error,config_error=nil,nil,nil
local state,restore_note='OFF','NONE'
local pending,camera_identity,journal=nil,nil,nil
local journal_session,trace,event_error=nil,nil,nil
local last_fallback,last_camera_distance=nil,nil
local report
local last_poll,last_log=-math.huge,-math.huge
local update_serial,processed_serial,update_at=0,-1,-math.huge
local active_since=nil
local key_was_down=false
local shoulder_was_down=false
local last_report={status='OFF'}
local clock=os.clock or os.time
local function now() return clock() end
local run_id='lua_'..string.format('%.6f',now())
local function save_settings()
    local ok,result=pcall(function() return json.dump_file(DIR..'settings.json',cfg) end)
    if not ok or result==false then config_error='ERROR_CONFIG_SAVE' else config_error=nil end
end
do
    local ok,value=pcall(function() return json.load_file(DIR..'settings.json') end)
    if ok then cfg=Core.settings(value) end
end
local function is_menu_open()
    local ok,v=pcall(function() return reframework:is_drawing_ui() end)
    return not ok or v~=false
end
local function has_method(obj,name)
    local ty=obj:get_type_definition()
    for _=1,16 do
        if not ty then return false end
        if ty:get_method(name) then return true end
        ty=ty:get_parent_type()
    end
    return false
end
local function camera()
    if not sdk or type(sdk.get_primary_camera)~='function' then return nil,'WAIT_CAMERA_API' end
    local cam=sdk.get_primary_camera()
    if not cam then return nil,'WAIT_NO_CAMERA' end
    if not has_method(cam,'get_GameObject') then return nil,'WAIT_GAMEOBJECT_METHOD' end
    local owner=cam:call('get_GameObject')
    if not owner then return nil,'WAIT_NO_GAMEOBJECT' end
    if not has_method(owner,'get_Transform') then return nil,'WAIT_TRANSFORM_METHOD' end
    local tr=owner:call('get_Transform')
    if not tr then return nil,'WAIT_NO_TRANSFORM' end
    local ty=tr:get_type_definition()
    if not ty or not ty:is_a('via.Transform') then return nil,'WAIT_TRANSFORM_TYPE' end
    return {id=tostring(cam:get_address())..':'..tostring(tr:get_address()),tr=tr}
end
local function position_of(c)
    local p=c.tr:get_position()
    if not p then return nil end
    local xyz={p.x,p.y,p.z}
    return Core.vector(xyz,3,1000000) and xyz or nil
end
local function set_position(c,p)
    c.tr:set_position(Vector4f.new(p[1],p[2],p[3],1),false)
end
local function latch(reason)
    fault=reason;requested=false;active_since=nil;obstacle:reset();gate:suspend(reason)
    if report then report(reason) else state=reason end
end
-- A failed setter may still mutate. Only its immediate finite readback on the
-- same current camera can replace the rollback comparison value.
local function remember_mutation(owned,p)
    if not p then return end
    local ok,current=pcall(camera)
    if ok and current and current.id==owned.id then owned.target=p end
end
local function restore_owned()
    -- Epoch preparation belongs to the model failure boundary too. Core retains
    -- health-invalid native snapshots for reporting; their player may be scalar.
    head_session:run(function()
        local s=gate.native
        local p=s and type(s.player)=='table' and s.player or nil
        local generation=p and p.generation
        local t=now()
        local epoch=s and s.phase=='MONITORING' and s.build=='KNOWN_EXE_HASH_AND_PE'
            and gate.fresh and t>=gate.changed_at and t-gate.changed_at<=0.35
            and p and p.status=='READABLE_CANDIDATE'
            and Core.finite(generation,4294967295) and generation>0 and generation%1==0
            and (s.session..':'..generation) or nil
        head_session:configure(cfg.ethan_head,epoch,cfg.full_model_pack)
        head_session:before_restore()
    end)
    if not pending then return true end
    local owned=pending
    local ok,c=pcall(camera)
    if not ok then latch('FAULT_RESTORE_LOOKUP');return false end
    if not c then
        restore_note='WAIT_RESTORE_CAMERA'
        if now()-owned.time>1 then pending=nil;latch('FAULT_RESTORE_EXPIRED') end
        return false
    end
    if c.id~=owned.id then pending=nil;restore_note='SKIPPED_DIFFERENT_CAMERA';return true end
    local read_ok,p=pcall(position_of,c)
    if not read_ok or not p then latch('FAULT_RESTORE_READ');return false end
    if Core.distance(p,owned.base)<=0.001 then
        pending=nil;restore_note='ALREADY_RESTORED';return true
    end
    if Core.distance(p,owned.target)>0.001 then
        pending=nil;restore_note='OWNERSHIP_LOST';latch('FAULT_CAMERA_OWNERSHIP_LOST');return false
    end
    local write_ok=pcall(set_position,c,owned.base)
    local check_ok,check=pcall(position_of,c)
    if not write_ok or not check_ok or not check or Core.distance(check,owned.base)>0.001 then
        if check_ok then remember_mutation(owned,check) end
        latch('FAULT_RESTORE_WRITE');restore_note='RESTORE_FAILED';return false
    end
    pending=nil;restore_note='RESTORED';return true
end
local function choose_enabled(value)
    if value then
        if pending or callback_error then return end
        fault=nil;requested=true;active_since=nil
    else requested=false;active_since=nil end
end
local function on_update()
    update_serial=update_serial+1;update_at=now()
    restore_owned()
end
local function flush_events()
    if trace and journal then trace:drain(function(event) return journal:record(event) end) end
end
local function transition_context(c,p)
    local n=gate.native or {}
    local player=type(n.player)=='table' and n.player or {}
    local aim=type(n.aim)=='table' and n.aim or {}
    local inventory=type(n.inventory)=='table' and n.inventory or {}
    local interact=type(n.interact)=='table' and n.interact or {}
    local distance=p and Core.vector(player.xyz,3,1000000) and Core.distance(p,player.xyz) or nil
    if distance then last_camera_distance=distance end
    return {requested=requested,native_seq=n.seq,native_tick_ms=n.tick_ms,native_reason=gate.reason,
        native_age_s=gate.changed_at and now()-gate.changed_at or nil,foreground=n.foreground,
        player_generation=player.generation,aim_generation=aim.generation,
        inventory_generation=inventory.generation,aim=aim.raw,inventory=inventory.raw,
        interact_slot=interact.slot_hex,player_step=gate.player_step,
        camera_id=c and c.id or camera_identity,camera_player_distance=distance,
        last_camera_player_distance=last_camera_distance,update_age_s=now()-update_at,restore=restore_note}
end
report=function(reason,c,p,target,after)
    if reason~='ACTIVE' and reason~='ENTERING' and reason~='FIRST_PERSON_COLLISION' then
        obstacle:reset()
    end
    local was_active=state=='ACTIVE' or state=='ENTERING'
    if was_active and reason~='ACTIVE' and reason~='ENTERING' and reason~='OFF' then
        last_fallback=reason
    end
    state=reason
    -- Evidence collection must not participate in camera failure handling.
    local event_ok=pcall(function()
        if trace then
            trace:observe('camera',reason,now(),transition_context(c,p))
            local m=head_session:status()
            trace:observe('model',m.state,now(),{binding=m.binding,enabled=m.enabled})
        end
    end)
    if not event_ok then event_error='ERROR_EVENT' end
    last_report={status=reason,camera_id=c and c.id or nil,base=p,target=target,after=after,
        restore=restore_note,requested=requested,native=gate.native,settings=cfg,
        collision=collision_report,lua_clock_s=now()}
end
local function on_camera()
    local t=now()
    if processed_serial==update_serial then
        local ready,why=gate:ready(t,cfg)
        local reason=not requested and 'OFF' or is_menu_open() and 'PAUSED_MENU'
            or t-update_at>0.25 and 'WAIT_UPDATE_CALLBACK' or not ready and why or nil
        if reason then
            restore_owned();active_since=nil;report(fault or reason)
        end
        return
    end
    processed_serial=update_serial
    if not restore_owned() then report(fault or 'WAIT_RESTORE_CAMERA');gate:suspend(state);return end
    if fault or callback_error then report(fault or callback_error);return end
    if not requested then report('OFF');active_since=nil;return end
    if update_serial==0 or t-update_at>0.25 then report('WAIT_UPDATE_CALLBACK');gate:suspend(state);return end
    if is_menu_open() then report('PAUSED_MENU');gate:suspend(state);active_since=nil;return end
    local ready,why=gate:ready(t,cfg)
    if not ready then report(why);active_since=nil;return end
    local c,reason=camera()
    if not c then report(reason);gate:suspend(reason);active_since=nil;return end
    if camera_identity~=c.id then
        camera_identity=c.id;gate:suspend('WARMUP_CAMERA');active_since=nil;report('WARMUP_CAMERA',c);return
    end
    local base=position_of(c)
    if not base or Core.distance(base,gate.native.player.xyz)>3 then
        report('WAIT_CAMERA_DISTANCE',c,base);gate:suspend(state);active_since=nil;return
    end
    local q=c.tr:get_rotation()
    local quat=q and {q.x,q.y,q.z,q.w} or nil
    if not active_since then active_since=t end
    local blend=math.min(1,(t-active_since)/cfg.entry_blend)
    local target=Core.offset(base,quat,cfg,blend)
    if not target then report('WAIT_CAMERA_VALUES',c,base);gate:suspend(state);active_since=nil;return end
    if blend==0 then report('ENTERING',c,base,base,base);return end
    if cfg.collision then
        local result=physics:cast(base,target)
        local status
        target,status=obstacle:limit(base,target,result,t,
            gate.session..':'..gate.native.player.generation..':'..c.id,cfg.collision_clearance)
        collision_report={status=status,query=result}
        if not target then
            active_since=nil;report('WAIT_COLLISION_'..(result.reason or 'UNAVAILABLE'),c,base);return
        end
        if status=='BLOCKED' then report('FIRST_PERSON_COLLISION',c,base);return end
    else
        obstacle:reset();collision_report={status='DISABLED'}
    end
    -- Capture ownership before invoking a setter: even an exception may have mutated.
    pending={id=c.id,base=base,target=target,time=t}
    local ok=pcall(set_position,c,target)
    local read_ok,after=pcall(position_of,c)
    if not ok or not read_ok or not after or Core.distance(after,target)>0.001 then
        if read_ok then remember_mutation(pending,after) end
        latch('FAULT_APPLY_WRITE');restore_owned();report(fault,c,base,target,after);return
    end
    local current=camera()
    if not current or current.id~=c.id then
        restore_owned();gate:suspend('WAIT_CAMERA_CHANGED');active_since=nil;report('WAIT_CAMERA_CHANGED');return
    end
    head_session:after_apply(after)
    report('ACTIVE',c,base,target,after)
end
local function on_frame()
    local t=now()
    if t-last_poll>=0.05 then
        last_poll=t
        local ok,s=pcall(function() return json.load_file(DIR..'status.json') end)
        gate:ingest(ok and s or nil,t)
        if gate.session and gate.native and journal_session~=gate.session then
            flush_events()
            if journal then journal:close('SESSION_CHANGED') end
            journal_session=gate.session
            trace=Trace.new(journal_session,run_id)
            journal=Journal.new(io,json,DIR,{kind='header',schema=1,version=VERSION,session=journal_session,run=run_id,
                camera_writes=true,native_game_writes=false,inline_hooks=false,
                pairing='latest native snapshot, not simultaneous',callback='pre LockScene / pre UpdateBehavior',
                collision='optional_experimental_center_ray',model_binding='experimental_rt_drawself'})
        end
    end
    local ok,down=pcall(function() return reframework:is_key_down(0x75) end)
    down=ok and down==true
    local s=gate.native
    if down and not key_was_down then
        if requested then choose_enabled(false)
        elseif s and s.foreground and gate.fresh and t-gate.changed_at<=0.35 and not is_menu_open() then
            choose_enabled(true)
        end
    end
    key_was_down=down
    local shoulder_ok,shoulder_down=pcall(function() return reframework:is_key_down(0x76) end)
    shoulder_down=shoulder_ok and shoulder_down==true
    if shoulder_down and not shoulder_was_down and requested and s and s.foreground
        and gate.fresh and t>=gate.changed_at and t-gate.changed_at<=0.35 and not is_menu_open() then
        cfg.shoulder=-cfg.shoulder;obstacle:reset();save_settings()
    end
    shoulder_was_down=shoulder_down
    flush_events()
    if journal and t-last_log>=0.2 then
        last_log=t
        journal:record({kind='sample',schema=1,session=journal_session,run=run_id,lua_clock_s=t,
            requested=requested,state=state,callback_error=callback_error,fault=fault,
            native_now=gate.native,camera=last_report,model=head_session:status(),
            events=trace and trace:stats() or nil,event_error=event_error})
    end
end
local function draw_ui()
    if not imgui.tree_node('RE7 OTS Explore v'..VERSION) then return end
    local changed,value=imgui.checkbox('Exploration camera (F6)',requested)
    if changed then choose_enabled(value) end
    imgui.text('State: '..(callback_error or fault or state))
    local _,native_reason=gate:ready(now(),cfg)
    imgui.text('Native: '..native_reason..' | Restore: '..restore_note)
    imgui.text('Last fallback: '..(last_fallback or 'NONE'))
    if event_error then imgui.text(event_error) end
    imgui.text('Automatic play log: '..(journal and journal.state or 'WAIT_NATIVE'))
    if config_error then imgui.text(config_error) end
    local dirty=false
    changed,value=imgui.checkbox('RT head model (experimental)',cfg.ethan_head)
    if changed then cfg.ethan_head=value;dirty=true end
    changed,value=imgui.checkbox('Use full RT model pack (all supported characters)',cfg.full_model_pack)
    if changed then cfg.full_model_pack=value;dirty=true end
    changed,value=imgui.checkbox('Obstacle avoidance (experimental center ray)',cfg.collision)
    if changed then cfg.collision=value;dirty=true end
    imgui.text('Collision: '..collision_report.status)
    local model=head_session:status()
    imgui.text('Head: '..model.state..' | '..model.binding)
    for _,v in ipairs({{'distance','Distance',0.2,2},{'shoulder','Shoulder',-0.6,0.6},{'height','Height',-0.3,0.6},
        {'collision_clearance','Wall clearance',0.03,0.3},
        {'aim_release_delay','Aim release hold (seconds)',0,1.5},{'entry_blend','Camera return blend (seconds)',0.05,0.8}}) do
        changed,value=imgui.slider_float(v[2],cfg[v[1]],v[3],v[4])
        if changed then cfg[v[1]]=value;dirty=true end
    end
    if imgui.button('Swap shoulder (F7)') then cfg.shoulder=-cfg.shoulder;dirty=true end
    if imgui.button('Reset offsets') then
        local defaults=Core.settings(nil)
        for _,name in ipairs({'distance','shoulder','height'}) do cfg[name]=defaults[name] end
        dirty=true
    end
    if dirty then cfg=Core.settings(cfg);obstacle:reset();save_settings() end
    imgui.text('Exploration prototype. Aim/inventory/interaction pause the camera.')
    imgui.text('Install the matching RT head package separately before enabling model mode.')
    imgui.text('Center ray is experimental; camera edges can still clip. F6 OFF for scripted scenes.')
    imgui.text('Logs: reframework/data/RE7_OTS_Explore (play_0 + play_1)')
    imgui.tree_pop()
end
local function guarded(fn)
    return function()
        local ok=pcall(fn)
        if not ok then latch('FAULT_CALLBACK_EXCEPTION');pcall(restore_owned) end
    end
end
local function register(name,fn,entry)
    if not re or type(re[name])~='function' then callback_error='WAIT_CALLBACK_API';return end
    local ok,result
    if entry then ok,result=pcall(re[name],entry,guarded(fn)) else ok,result=pcall(re[name],guarded(fn)) end
    if not ok or result==false then callback_error='WAIT_CALLBACK_REGISTRATION' end
end
local ok,game=pcall(function() return reframework:get_game_name() end)
if not ok or type(game)~='string' or game:lower()~='re7' then callback_error='WAIT_RE7_RUNTIME' end
register('on_pre_application_entry',on_update,'UpdateBehavior')
register('on_pre_application_entry',on_camera,'LockScene')
register('on_frame',on_frame)
register('on_draw_ui',draw_ui)
register('on_script_reset',function()
    requested=false;restore_owned();report(fault or 'OFF');flush_events();save_settings()
    if journal then journal:close('SCRIPT_RESET') end
end)
if re and re.on_config_save then register('on_config_save',save_settings) end
if log and log.info then log.info('[RE7_EXPLORE] v'..VERSION..' loaded OFF. F6 camera / F7 shoulder; logs automatic.') end
