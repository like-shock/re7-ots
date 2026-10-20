import unittest
from test_ethan_model import module
from lua_runner import run_lua

HARNESS=r'''
local function vec(x,y,z,w) return {x=x,y=y,z=z,w=w} end
Vector3f={new=vec}
local time=1
local writes={}
local ctl_pos={500,400,0}
local anchor=vec(0,0,-2)
local foreign=false
local ignore=false
local fail=false
local objects={}
local function object(id,kind,methods,fields)
 local o={id=id,methods=methods,fields=fields or {}}
 function o:get_address() return self.id end
 function o:get_type_definition() return {is_a=function(_,t) return t==kind end} end
 function o:call(n,...) assert(self.methods[n],n);return self.methods[n](...) end
 function o:get_field(n) return self.fields[n] end
 objects[id]=o;return o
end
local view=object(3,'via.gui.View',{get_ViewType=function() return 0 end,get_ScreenSize=function() return {w=1000,h=800} end})
local control=object(4,'via.gui.TransformObject',{
 get_Position=function() return vec(table.unpack(ctl_pos)) end,
 get_Parent=function() return view end,
 set_Position=function(p)
  writes[#writes+1]={p.x,p.y,p.z}
  if not ignore then ctl_pos={p.x,p.y,p.z} end
  if fail then fail=false;error('partial setter') end
 end})
local cache={get_field=function(_,n) assert(n=='_Element');return control end}
local go,attach
attach=object(5,'app.UIWorldPosAttach',{get_GameObject=function() return go end}, {_TargetGUIElem=cache})
attach.get_field=function(_,n) if n=='_NowTargetPos' then return anchor else return cache end end
go=object(2,'via.GameObject',{get_Name=function() return 'InteractOperationCursor' end,
 ['getComponent(System.Type)']=function(t) assert(t=='app.UIWorldPosAttach');return not foreign and attach end})
local gui=object(1,'via.gui.GUI',{get_GameObject=function() return go end,get_View=function() return view end})
local sdk={typeof=function(t) return t end,find_type_definition=function(t)
 assert(t=='via.gui.ViewType');return {get_field=function(_,n) assert(n=='Screen');return {get_data=function() return 0 end} end}
end}
local p=Prompt.new(sdk,function() return time end)
local projection={{1,0,0,0},{0,1,0,0},{0,0,-1,-1},{0,0,-0.2,0}}
local context={base={0,0,0},target={0.5,0,1},rotation={0,0,0,1},projection=projection,time=time,serial=1}
p:configure(true,context)
'''

class PromptTests(unittest.TestCase):
 def check(self,s):
  run_lua(module('explore/core','Core')+module('explore/prompt','Prompt')+HARNESS+s)
 def test_project_and_reproject_preserve_depth_and_restore(self):
  self.check('''
assert(p:pre(gui)==true);assert(math.abs(ctl_pos[1]-416.6666667)<0.001);assert(ctl_pos[2]==400 and ctl_pos[3]==0)
assert(p:post(gui));assert(ctl_pos[1]==500 and #writes==2)
for i=1,20 do p:pre(gui);p:post(gui) end
assert(ctl_pos[1]==500,'no cumulative correction')
context.target={-0.5,0,1};p:configure(true,context);p:pre(gui)
assert(math.abs(ctl_pos[1]-583.333333)<0.001);p:post(gui)
''')
 def test_wrong_layout_already_correct_offscreen_or_stale_are_not_written(self):
  self.check('''
ctl_pos[1]=123;p:pre(gui);assert(#writes==0 and p:status().state=='UNSUPPORTED_LAYOUT')
ctl_pos[1]=416.6666667;p:pre(gui);assert(#writes==0 and p:status().state=='ALREADY_ALIGNED')
ctl_pos[1]=500;anchor=vec(0,0,2);p:pre(gui);assert(#writes==0)
anchor=vec(0,0,-2);time=2;p:pre(gui);assert(#writes==0)
''')
 def test_missing_post_disable_and_external_owner_are_handled(self):
  self.check('''
p:pre(gui);p:pre(gui);assert(math.abs(ctl_pos[1]-416.6666667)<0.001)
p:configure(false,nil);assert(p:restore_all());assert(ctl_pos[1]==500)
p:configure(true,context);p:pre(gui);ctl_pos[1]=222;p:post(gui)
assert(ctl_pos[1]==222,'external writer survives');assert(p:status().state=='OWNERSHIP_LOST')
''')
 def test_partial_setter_ignored_setter_and_replacement(self):
  self.check('''
fail=true;p:pre(gui);assert(ctl_pos[1]==500,'partial setter restored')
ignore=true;p:pre(gui);assert(ctl_pos[1]==500);ignore=false
p:pre(gui);local old=control
control=object(7,'via.gui.TransformObject',old.methods)
local count=#writes;p:post(gui);assert(#writes==count,'never restore into replacement control')
''')
 def test_parent_transform_is_rejected_and_other_gui_untouched(self):
  self.check('''
local parent=object(8,'via.gui.TransformObject',{get_Position=function() return vec(2,0,0) end,
 get_Scale=function() return vec(1,1,1) end,get_Rotation=function() return 0 end,get_Parent=function() return view end})
control.methods.get_Parent=function() return parent end
p:pre(gui);assert(#writes==0)
go.methods.get_Name=function() return 'Menu' end;p:pre(gui);assert(#writes==0)
''')
 def test_missing_parent_api_and_unproven_view_ownership_are_rejected(self):
  for parent in ['nil', 'function() return nil end', "function() return object(99,'via.gui.View',{}) end"]:
   with self.subTest(parent=parent):
    self.check('''
control.methods.get_Parent='''+parent+'''
control.call=function(self,n,...) if self.methods[n] then return self.methods[n](...) end end
p:pre(gui);assert(#writes==0,'unverified layout must not authorize a screen write')
''')
 def test_nested_draw_keeps_outer_correction_until_outer_post(self):
  self.check('''
p:pre(gui);p:pre(gui);assert(p:post(gui))
assert(math.abs(ctl_pos[1]-416.666667)<0.001,'nested post must preserve outer drawing pose')
assert(p:post(gui));assert(ctl_pos[1]==500)
''')
 def test_projection_handles_rotated_camera_and_invalid_matrices(self):
  self.check('''
local q={0,math.sqrt(0.5),0,math.sqrt(0.5)}
local result=Prompt.project({-2,0,0},{0,0,0},q,projection,1280,800)
assert(result and math.abs(result[1]-640)<0.001 and result[2]==400)
assert(not Prompt.project({2,0,0},{0,0,0},q,projection,1280,800))
projection[1][1]=0/0;assert(not Prompt.project({0,0,-2},{0,0,0},{0,0,0,1},projection,1280,800))
''')
