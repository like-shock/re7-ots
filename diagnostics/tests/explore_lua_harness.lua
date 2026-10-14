local tick,seq=0,0
local phase='frame'
local fixture=nil
local callbacks={}
local texts,records,files={},{},{}
local position={1,3,3}
local rotation={0,0,0,1}
local camera_id,transform_id=10,20
local key,shoulder_key,menu_open=false,false,false
local nil_camera,fail_camera,fail_set,fail_after_set,ignore_set=false,false,false,false,false
local fail_io,fail_json=false,false
local partial_set,partial_restore=false,false
local set_calls=0
local ui_enable,ui_head=nil,nil
local saved=nil
local config={schema=1,distance=1,shoulder=0.25,height=0.2,enabled=true}
os.clock=function() return tick end
local function copy(t) if type(t)~='table' then return t end;local r={};for k,v in pairs(t) do r[k]=copy(v) end;return r end
json={load_file=function(p) if p:find('settings',1,true) then return config end;if fail_json then error('partial') end;return fixture end,
 dump_string=function(t) records[#records+1]=copy(t);return '{"mock":true}' end,
 dump_file=function(_,value) saved=copy(value);return true end}
io={open=function(path,mode) assert(mode=='wb');if fail_io then return nil end;files[path]='';return {
 write=function(self,s) if fail_io then return nil end;files[path]=files[path]..s;return self end,
 flush=function() return not fail_io end,close=function() return true end} end}
Vector4f={new=function(x,y,z,w) return {x=x,y=y,z=z,w=w} end}
local component_ty={get_method=function(_,n) return n=='get_GameObject' and {} or nil end,get_parent_type=function() return nil end}
local camera_ty={get_method=function() return nil end,get_parent_type=function() return component_ty end}
local owner_ty={get_method=function(_,n) return n=='get_Transform' and {} or nil end,get_parent_type=function() return nil end}
local tr={get_address=function() return transform_id end,get_type_definition=function() return {is_a=function(_,n) return n=='via.Transform' end} end,
 get_position=function() return {x=position[1],y=position[2],z=position[3],w=1} end,
 get_rotation=function() return {x=rotation[1],y=rotation[2],z=rotation[3],w=rotation[4]} end,
 set_position=function(_,p,no_dirty)
  assert(phase=='update' or phase=='lock' or phase=='reset','setter outside engine callback')
  assert(no_dirty~=true,'scene not locked at configured callbacks');set_calls=set_calls+1
  if fail_set then error('setter failed') end
  if not ignore_set then position={p.x,p.y,p.z} end
  if partial_set then partial_set=false;position[3]=p.z-0.4;error('partial apply') end
  if partial_restore then partial_restore=false;position[3]=p.z+0.4;error('partial restore') end
  if fail_after_set then fail_after_set=false;error('setter changed position then threw') end
 end}
local owner={get_type_definition=function() return owner_ty end,call=function(_,n) assert(n=='get_Transform');return tr end}
local cam={get_address=function() return camera_id end,get_type_definition=function() return camera_ty end,call=function(_,n) assert(n=='get_GameObject');return owner end}
sdk={get_primary_camera=function()
 assert(phase=='update' or phase=='lock' or phase=='reset','SDK read outside engine callback')
 if fail_camera then error('bad camera') end;return not nil_camera and cam or nil
end}
re={on_frame=function(f) callbacks.frame=f end,on_pre_application_entry=function(n,f) callbacks[n]=f end,
 on_script_reset=function(f) callbacks.reset=f end,on_draw_ui=function(f) callbacks.ui=f end,on_config_save=function(f) callbacks.save=f end}
reframework={get_game_name=function() return 're7' end,is_key_down=function(_,k)
 assert(k==0x75 or k==0x76);return k==0x75 and key or k==0x76 and shoulder_key
end,is_drawing_ui=function() return menu_open end}
imgui={tree_node=function() return true end,tree_pop=function() end,text=function(s) texts[#texts+1]=s end,
 checkbox=function(label,v) if label=='RT head model (experimental)' then
  if ui_head~=nil then local r=ui_head;ui_head=nil;return true,r end;return false,v end
  if ui_enable~=nil then local r=ui_enable;ui_enable=nil;return true,r end;return false,v end,
 slider_float=function(_,v) return false,v end,button=function() return false end}
log={info=function() end,error=function() end}
-- INSERT ADAPTER HERE
local function step(change,skip_update)
 tick=tick+0.1;seq=seq+1;fixture=snapshot(seq);if change then change(fixture) end
 phase='frame';callbacks.frame()
 if not skip_update then phase='update';if callbacks.UpdateBehavior then callbacks.UpdateBehavior() end end
 phase='lock';if callbacks.LockScene then callbacks.LockScene() end
 phase='frame'
end
local function ui() phase='ui';texts={};callbacks.ui();phase='frame';return table.concat(texts,'|') end
local function start()
 for i=1,10 do step() end
 key=true;step();key=false;for i=1,10 do step() end
end
local function near(a,b) return math.abs(a-b)<0.002 end
-- CASES BEGIN
