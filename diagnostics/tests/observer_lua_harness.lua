local tick, fixture = 0, nil
local frame, app, reset, ui
local files, records, texts = {}, {}, {}
local fail_write, fail_json, fail_camera, missing_method, nil_camera, bad_position = false,false,false,false,false,false
local cam_id, transform_id = 10,20
local nil_owner, missing_owner_method = false,false
local owner_calls, transform_calls = 0,0
os.clock=function() return tick end
json={load_file=function() if fail_json then error('partial JSON') end return fixture end,
 dump_string=function(t) records[#records+1]=t; return '{"mock_serialized":true}' end}
io={open=function(path,mode)
 if mode=='rb' then if not files[path] then return nil end return {close=function() return true end} end
 assert(mode=='wb');assert(not files[path]);files[path]=''
 return {write=function(self,line) if fail_write then return nil end files[path]=files[path]..line;return self end,flush=function() return not fail_write end,close=function() return true end}
end}
re={on_frame=function(f) frame=f end,on_application_entry=function(name,f) assert(name=='UpdateBehavior');app=f end,on_script_reset=function(f) reset=f end,on_draw_ui=function(f) ui=f end}
log={info=function() end}
imgui={tree_node=function() return true end,text=function(s) texts[#texts+1]=s end,tree_pop=function() end}
local ty={is_a=function(_,name) return name=='via.Transform' end}
local tr={get_type_definition=function() return ty end,get_address=function() return transform_id end,
 get_position=function() return {x=bad_position and 0/0 or 1,y=2,z=3} end,get_rotation=function() return {x=0,y=0,z=0,w=1} end}
local owner_ty={get_method=function(_,name) return name=='get_Transform' and not missing_method and {} or nil end,get_parent_type=function() return nil end}
local owner={get_type_definition=function() return owner_ty end,
 call=function(_,name) assert(name=='get_Transform');transform_calls=transform_calls+1;return tr end}
-- Camera inherits get_GameObject from Component; Transform belongs to GameObject.
local component_ty={get_method=function(_,name) return name=='get_GameObject' and not missing_owner_method and {} or nil end,get_parent_type=function() return nil end}
local camera_ty={get_method=function() return nil end,get_parent_type=function() return component_ty end}
local cam={get_type_definition=function() return camera_ty end,get_address=function() return cam_id end,
 call=function(_,name) assert(name=='get_GameObject');owner_calls=owner_calls+1;return not nil_owner and owner or nil end}
sdk={get_primary_camera=function() if fail_camera then error('dead object') end return not nil_camera and cam or nil end}
-- INSERT ADAPTER HERE
local function status(seq,session,phase)
 fixture={schema=1,version='0.2.0',session=session or 're7_observer_2026_10_13_12_0_0_1000_1',seq=seq,tick_ms=seq*100,stage='STILL',phase=phase or 'RECORDING'}
end
local function step() tick=tick+0.11;frame();app() end
local function last_sample()
 for i=#records,1,-1 do if records[i].kind=='sample' then return records[i] end end
end
-- CASES BEGIN
step();assert(next(files)==nil)
status(1);step();assert(next(files)==nil,'old status must not start a capture')
status(2);step();assert(last_sample().camera.status=='READABLE_CANDIDATE');assert(last_sample().camera.xyz[3]==3);assert(last_sample().camera.generation==1)
local n=#records;app();assert(#records==n,'rate limit');step();assert(#records==n,'deduplicate native sample')
cam_id=11;status(3);step();assert(last_sample().camera.generation==2)
nil_camera=true;status(4);step();assert(last_sample().camera.status=='UNKNOWN_NO_CAMERA')
nil_camera=false;status(5);step();assert(last_sample().camera.generation==3)
fail_camera=true;status(6);step();assert(last_sample().camera.status=='UNKNOWN_SDK_EXCEPTION')
fail_camera=false;missing_method=true;status(7);step();assert(last_sample().camera.status=='UNKNOWN_TRANSFORM_METHOD')
missing_method=false;bad_position=true;status(8);step();assert(last_sample().camera.status=='UNKNOWN_POSITION_VALUE')
bad_position=false
n=#records;tick=tick+3;frame();app();assert(#records==n,'stale native must stop reads')
fail_json=true;step();assert(#records==n,'partial status must stop reads');fail_json=false
status(9);step();assert(last_sample().camera.status=='READABLE_CANDIDATE')
status(10,nil,'STOPPED');step();assert(records[#records].kind=='end')
local old_files=0;for _ in pairs(files) do old_files=old_files+1 end
status(11);step();status(12);step();local new_files=0;for _ in pairs(files) do new_files=new_files+1 end;assert(new_files==old_files,'finished session stays finished')
local second='re7_observer_2026_10_13_12_0_0_2000_2'
status(1,second);step();status(2,second);step();assert(last_sample().session==second)
fail_write=true;status(3,second);step();fail_write=false
n=#records;status(4,second);step();assert(#records==n,'failed file must not keep writing')
ui();assert(table.concat(texts,'|'):find('ERROR',1,true))
reset()
print('Lua observer: nil, replacement, errors, stale status, throttling, recovery and I/O checks PASS')
