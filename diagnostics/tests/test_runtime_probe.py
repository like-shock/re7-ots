import unittest
from test_ethan_model import module
from lua_runner import run_lua

FIXTURE = '''
local calls=0
local target={get_type_definition=function() return {is_a=function(_,t) return t=='via.Transform' end} end,
 get_position=function() return {x=1,y=2,z=3} end}
local ik={get_field=function(_,n) assert(n=='Target');return target end}
local count=2
local array={get_size=function() return count end,get_element=function(_,i) calls=calls+1;assert(i<2);return ik end}
local player,hand,motion
player={get_address=function() return 7 end,call=function(_,n,t)
 assert(n=='getComponent(System.Type)');return t=='app.PlayerHandTouch' and hand or motion end}
hand={call=function(_,n) assert(n=='get_GameObject');return player end,
 get_field=function(_,n) assert(n=='HandIK');return array end}
motion={call=function(_,n) assert(n=='get_GameObject');return player end,
 get_field=function(_,n) return n=='IsPosturalCameraControl' end}
local sdk={typeof=function(n) return n end,get_managed_singleton=function(n)
 assert(n=='app.ObjectManager');return {get_field=function(_,f) assert(f=='PlayerObj');return player end} end}
'''


class RuntimeProbeTests(unittest.TestCase):
    def probe(self, tail):
        run_lua(module('explore/core','Core')+module('explore/probe','Probe')+FIXTURE+tail)

    def test_hand_targets_are_copied_and_never_return_sdk_objects(self):
        self.probe('''
local r=Probe.player(sdk);assert(r.hands.status=='OK');assert(#r.hands.targets==2)
assert(r.hands.targets[1][3]==3 and calls==2)
assert(r.motion.IsRArmJacked==false and r.motion.IsPosturalCameraControl==true)
local function scalar(t) assert(type(t)=='table');for k,v in pairs(t) do
 assert(type(k)=='string' or type(k)=='number')
 if type(v)=='table' then scalar(v) else assert(type(v)=='number' or type(v)=='boolean' or type(v)=='string') end
end end;scalar(r)
''')

    def test_array_bounds_and_wrong_component_owner_never_get_elements(self):
        self.probe('''
count=1000;local r=Probe.player(sdk);assert(r.hands.status~='OK' and calls==0)
hand.call=function() return {get_address=function() return 99 end} end
count=2;r=Probe.player(sdk);assert(r.hands.status~='OK' and calls==0)
''')

    def test_camera_joint_is_optional_and_bad_values_are_not_logged(self):
        self.probe('''
local pos={x=1,y=2,z=3};local tr={get_position=function() return pos end,
 get_rotation=function() return {x=0,y=0,z=0,w=1} end,
 call=function(_,n,j) assert(n=='getJointByName' and j=='Camera');return nil end}
local r=Probe.camera({id='1:2',tr=tr});assert(r.transform[3]==3)
pos.z=99;assert(r.transform[3]==3,'copy values');pos.x=0/0
r=Probe.camera({id='1:2',tr=tr});assert(r.transform==nil)
''')

    def test_missing_sdk_and_throwing_fields_return_diagnostic_status_only(self):
        self.probe('''
assert(Probe.player({}).status~='OK')
hand.get_field=function() error('unavailable') end
local r=Probe.player(sdk);assert(r.hands.status~='OK');assert(r.motion.IsPosturalCameraControl)
''')
