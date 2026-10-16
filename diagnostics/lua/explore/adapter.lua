-- Exploration camera with opt-in RT models and experimental obstacle avoidance.
local VERSION='0.5.8-dev'
local DIR='RE7_OTS_Explore/'
local gate=Core.new()
local head_session=ModelGroup.new(sdk)
local physics=Physics.new(sdk)
local prompt=Prompt.new(sdk)
local prompt_available=false
local prompt_context=nil
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
local early_serial,locked_serial=-1,-1
local early_available=false
local phase_probe,probe_serial,last_probe=nil,-1,-math.huge
local key_was_down=false
local shoulder_was_down=false
local last_report={status='OFF'}
local native_read={status='NONE',attempts=0,recovered_total=0,failed_total=0}
local clock=os.clock or os.time
local function now() return clock() end
local run_id='lua_'..string.format('%.6f',now())
local function read_native()
    local r={attempts=0,recovered_total=native_read.recovered_total,failed_total=native_read.failed_total}
    local value
    for attempt=1,2 do
        local ok,s=pcall(function() return json.load_file(DIR..'status.json') end)
        r.attempts=attempt
        r.status=not ok and 'READ_ERROR' or s==nil and 'EMPTY' or type(s)=='table' and 'DECODED' or 'INVALID_TYPE'
        if ok and s~=nil then value=s;break end
        if attempt==1 then r.first_failure=r.status end
    end
    -- No cached data is substituted. Every decoded result still passes through
    -- the unchanged native identity, freshness and gameplay gates below.
    if value==nil then r.failed_total=r.failed_total+1
    elseif r.attempts==2 then r.recovered_total=r.recovered_total+1 end
    native_read=r
    return value
end
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
    return {id=tostring(cam:get_address())..':'..tostring(tr:get_address()),tr=tr,cam=cam}
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
    prompt_context=nil;prompt:configure(false,nil);prompt:restore_all()
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
local function observe_phase(name)
    if not phase_probe or probe_serial~=update_serial then return end
    -- Diagnostics cannot fault or delay camera cleanup. Each phase overwrites a
    -- fixed key; a duplicate callback cannot grow an unbounded list.
    local ok=pcall(function()
        local c=camera()
        phase_probe.phases[name]=Probe.camera(c)
    end)
    if not ok then phase_probe.phases[name]={status='UNAVAILABLE'} end
end
local function on_update()
    update_serial=update_serial+1;update_at=now()
    if not requested or not gate.fresh or update_at-gate.changed_at>0.35 then
        phase_probe=nil;probe_serial=-1
    elseif update_at-last_probe>=0.5 then
        last_probe=update_at;probe_serial=update_serial
        phase_probe={serial=update_serial,lua_clock_s=update_at,early_camera=cfg.early_camera,phases={}}
        observe_phase('update_before')
    end
    restore_owned()
    observe_phase('update_after_restore')
    if phase_probe and probe_serial==update_serial then
        if cfg.ethan_head and head_session.current_epoch then
            local ok,value=pcall(Probe.player,sdk)
            phase_probe.player=ok and value or {status='UNAVAILABLE'}
        else phase_probe.player={status='DISABLED_OR_NO_MODEL_EPOCH'} end
    end
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
        last_camera_player_distance=last_camera_distance,update_age_s=now()-update_at,restore=restore_note,
        native_read=native_read,collision_last=collision_report}
end
report=function(reason,c,p,target,after)
    if reason~='ACTIVE' and reason~='ENTERING' and reason~='FIRST_PERSON_COLLISION'
        and not reason:match('^WAIT_COLLISION_') then
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
        collision=collision_report,prompt=prompt:status(),lua_clock_s=now()}
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
    -- Collision is queried toward the full requested destination. Entry blend
    -- must not shorten the ray and make the same wall appear/disappear.
    local target=Core.offset(base,quat,cfg,cfg.collision and 1 or blend)
    if not target then report('WAIT_CAMERA_VALUES',c,base);gate:suspend(state);active_since=nil;return end
    if cfg.collision then
        local result=physics:cast(base,target)
        local status
        target,status=obstacle:limit(base,target,result,t,
            gate.session..':'..gate.native.player.generation..':'..c.id,cfg.collision_clearance,blend)
        collision_report={status=status,query=result,stabilization=obstacle:status(),lua_clock_s=t}
        if not target then
            active_since=nil;report('WAIT_COLLISION_'..(result.reason or 'UNAVAILABLE'),c,base);return
        end
        if status=='BLOCKED' then report('FIRST_PERSON_COLLISION',c,base);return end
    else
        obstacle:reset();collision_report={status='DISABLED',lua_clock_s=t}
    end
    if blend==0 then report('ENTERING',c,base,base,base);return end
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
    -- Copy matrix/pose scalars only. GUI projection failure cannot fault camera recovery.
    if cfg.prompt_correction and prompt_available then
        local ok,context=pcall(function()
            local m=c.cam:call('get_ProjectionMatrix');local projection={}
            for i=0,3 do local v=m[i];projection[i+1]={v.x,v.y,v.z,v.w} end
            return {id=c.id,base=base,target=after,rotation=quat,projection=projection,time=t,serial=update_serial}
        end)
        if ok then prompt_context=context end
    end
    report('ACTIVE',c,base,target,after)
end
local function on_frame()
    local t=now()
    if t-last_poll>=0.05 then
        last_poll=t
        gate:ingest(read_native(),t)
        if gate.session and gate.native and journal_session~=gate.session then
            flush_events()
            if journal then journal:close('SESSION_CHANGED') end
            journal_session=gate.session
            trace=Trace.new(journal_session,run_id)
            journal=Journal.new(io,json,DIR,{kind='header',schema=1,version=VERSION,session=journal_session,run=run_id,
                camera_writes=true,native_game_writes=false,inline_hooks=false,
                pairing='latest native snapshot, not simultaneous',callback='pre UpdateBehavior / optional pre PrerenderGUI / pre LockScene',
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
            native_now=gate.native,native_read=native_read,camera=last_report,model=head_session:status(),
            events=trace and trace:stats() or nil,event_error=event_error,prompt=prompt:status()})
        if cfg.prompt_correction then
            journal:record({kind='prompt',schema=1,session=journal_session,run=run_id,lua_clock_s=t,prompt=prompt:status(true)})
        end
        -- Scene evidence gets its own bounded record: combining native data,
        -- fractional phase coordinates and materials can exceed 4096 bytes.
        if cfg.ethan_head or phase_probe then
            local model=cfg.ethan_head and head_session:status(true) or nil
            if model then
                -- An acquired multi-part group may retain both group and part
                -- failures. Keep that evidence out of the populated scene row.
                local binding={diagnostic=model.diagnostic,parts={},epoch=model.epoch}
                local has_binding=model.diagnostic~=nil
                model.diagnostic=nil
                for name,part in pairs(model.parts) do
                    if part.diagnostic then
                        binding.parts[name]=part.diagnostic;has_binding=true;part.diagnostic=nil
                    end
                end
                if has_binding then
                    journal:record({kind='model_binding',schema=1,session=journal_session,run=run_id,lua_clock_s=t,
                        model=binding})
                end
                -- Per-instance material evidence has its own row, so additional
                -- verified head copies cannot overflow fractional phase data.
                for name,part in pairs(model.parts) do
                    journal:record({kind='model_part',schema=1,session=journal_session,run=run_id,lua_clock_s=t,
                        model={epoch=model.epoch,parts={[name]=part}}})
                    model.parts[name]={state=part.state,binding=part.binding}
                end
            end
            journal:record({kind='scene_detail',schema=1,session=journal_session,run=run_id,lua_clock_s=t,
                model=model,phase_probe=phase_probe})
        end
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
    changed,value=imgui.checkbox('Earlier HUD camera (experimental)',cfg.early_camera)
    if changed then cfg.early_camera=value;dirty=true end
    changed,value=imgui.checkbox('Prompt reprojection (experimental)',cfg.prompt_correction)
    if changed then cfg.prompt_correction=value;dirty=true end
    imgui.text('Prompt: '..(prompt_available and prompt:status().state or 'WAIT_DRAW_CALLBACK'))
    imgui.text('Collision: '..collision_report.status)
    local stability=collision_report.stabilization
    if stability and stability.blocked then
        imgui.text('Collision hold: '..stability.hold_reason..' | needs 0.4s stable space')
    end
    local model=head_session:status()
    imgui.text('Head: '..model.state..' | '..model.binding)
    imgui.text('Head instances: '..tostring(model.instances))
    if model.diagnostic_kind then imgui.text('Head binding detail: '..model.diagnostic_kind) end
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
    imgui.text('Prompt reprojection aligns supported A cursors; interaction selection / firing origins remain native.')
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
local function on_early_camera()
    observe_phase('gui_before')
    if cfg.early_camera and locked_serial~=update_serial and early_serial~=update_serial then
        early_serial=update_serial;on_camera()
    end
    observe_phase('gui_after')
end
local function on_lock_camera()
    locked_serial=update_serial
    observe_phase('lock_before')
    if cfg.early_camera then
        if not early_available or early_serial~=update_serial then
            restore_owned();active_since=nil;report(fault or 'WAIT_EARLY_CAMERA')
            observe_phase('lock_after');return
        end
        if pending then
            local c=camera()
            if not c or c.id~=pending.id then
                restore_owned();gate:suspend('WAIT_CAMERA_CHANGED');active_since=nil
                report(fault or 'WAIT_CAMERA_CHANGED');observe_phase('lock_after');return
            end
            local p=position_of(c)
            if not p or Core.distance(p,pending.target)>0.001 then
                -- An engine/other-mod write after early application must never
                -- become our next base or be overwritten with an old position.
                restore_owned();latch('FAULT_EARLY_CAMERA_CHANGED');report(fault)
                observe_phase('lock_after');return
            end
        end
    end
    on_camera()
    observe_phase('lock_after')
end
local ok,game=pcall(function() return reframework:get_game_name() end)
if not ok or type(game)~='string' or game:lower()~='re7' then callback_error='WAIT_RE7_RUNTIME' end
register('on_pre_application_entry',on_update,'UpdateBehavior')
register('on_pre_application_entry',on_lock_camera,'LockScene')
-- Optional callbacks must not make the established default path unavailable.
if re and type(re.on_pre_application_entry)=='function' then
    local registered,result=pcall(re.on_pre_application_entry,'PrerenderGUI',guarded(on_early_camera))
    early_available=registered and result~=false
    pcall(re.on_pre_application_entry,'BeginRendering',function() observe_phase('render') end)
end
-- GUI hooks operate on the final confirmed pose and restore their own screen
-- control immediately after draw. Unsupported layouts keep the game's cursor.
if re and type(re.on_pre_gui_draw_element)=='function' and type(re.on_gui_draw_element)=='function' then
    local post_ok,post_result=pcall(re.on_gui_draw_element,function(element) prompt:post(element) end)
    local pre_ok,pre_result=pcall(re.on_pre_gui_draw_element,function(element)
        local ok=pcall(function()
            if not prompt:is_candidate(element) then return end
            local context=prompt_context
            local ready=gate:ready(now(),cfg)
            local enabled=prompt_available and cfg.prompt_correction and requested and state=='ACTIVE'
                and context and context.serial==update_serial and ready and not is_menu_open()
            if enabled then
                local c=camera();local p=c and position_of(c)
                local q=c and c.tr:get_rotation()
                local rotation=q and {q.x,q.y,q.z,q.w}
                local error=0
                if Core.vector(rotation,4,2) then
                    for i=1,4 do error=error+(rotation[i]-context.rotation[i])^2 end
                else error=1 end
                enabled=c and c.id==context.id and p and Core.distance(p,context.target)<=0.001 and error<0.000001
            end
            prompt:configure(enabled==true,context)
            prompt:pre(element)
        end)
        if not ok then prompt:configure(false,nil);prompt:restore_all() end
        return true
    end)
    prompt_available=post_ok and post_result~=false and pre_ok and pre_result~=false
end
register('on_frame',on_frame)
register('on_draw_ui',draw_ui)
register('on_script_reset',function()
    requested=false;restore_owned();report(fault or 'OFF');flush_events();save_settings()
    if journal then journal:close('SCRIPT_RESET') end
end)
if re and re.on_config_save then register('on_config_save',save_settings) end
if log and log.info then log.info('[RE7_EXPLORE] v'..VERSION..' loaded OFF. F6 camera / F7 shoulder; logs automatic.') end
