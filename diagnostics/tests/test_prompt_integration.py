import unittest
import test_explore_lua as camera
from test_prompt import HARNESS

SETUP=HARNESS.split('local p=Prompt.new')[0]
SETUP=SETUP.replace('local sdk={typeof=function(t) return t end,find_type_definition=function(t)', 'sdk.typeof=function(t) return t end;sdk.find_type_definition=function(t)')
# End of the SDK table becomes the end of its assigned function.
SETUP=SETUP.rsplit('end}',1)[0]+'end\n'
SETUP += """
anchor=vec(1,3,1)
local original_cam_call=cam.call
cam.call=function(self,n,...)
 if n=='get_ProjectionMatrix' then return {
 [0]=vec(1,0,0,0),[1]=vec(0,1,0,0),[2]=vec(0,0,-1,-1),[3]=vec(0,0,-0.2,0)} end
 return original_cam_call(self,n,...)
end
re.on_pre_gui_draw_element=function(f) callbacks.pre_gui=f end
re.on_gui_draw_element=function(f) callbacks.post_gui=f end
"""

class PromptIntegrationTests(unittest.TestCase):
 adapter=camera.ExploreAdapterTests.adapter
 def test_final_camera_prompt_and_disable_restore(self):
  self.adapter("""
start();phase='gui';assert(callbacks.pre_gui,'register draw callback');callbacks.pre_gui(gui)
assert(math.abs(ctl_pos[1]-458.333333)<0.001 and math.abs(ctl_pos[2]-426.666667)<0.001)
callbacks.post_gui(gui);assert(ctl_pos[1]==500 and ctl_pos[2]==400)
callbacks.pre_gui(gui);phase='update';callbacks.UpdateBehavior();assert(ctl_pos[1]==500,'missing post cleaned before game update')
step(function(s) s.aim.raw=1 end);phase='gui';local n=#writes;callbacks.pre_gui(gui);assert(#writes==n)
""",SETUP)
 def test_current_camera_must_still_match_confirmed_pose(self):
  self.adapter("""
start();position[1]=position[1]+1;phase='gui';assert(callbacks.pre_gui);callbacks.pre_gui(gui)
assert(#writes==0,'do not project using a stale camera snapshot')
""",SETUP)
 def test_unrelated_gui_skips_camera_validation_but_owned_cursor_is_cleaned(self):
  self.adapter("""
start();phase='gui'
local original=sdk.get_primary_camera;local calls=0
sdk.get_primary_camera=function(...) calls=calls+1;return original(...) end
go.methods.get_Name=function() return 'UnrelatedHUD' end
for i=1,100 do callbacks.pre_gui(gui);callbacks.post_gui(gui) end
assert(calls==0 and #writes==0,'unrelated GUI must not resolve the primary camera')
go.methods.get_Name=function() return 'InteractOperationCursor' end
callbacks.pre_gui(gui);assert(calls==1 and #writes==1)
step(function(s) s.aim.raw=1 end)
assert(ctl_pos[1]==500 and ctl_pos[2]==400,'disabled correction must still restore ownership')
""",SETUP)
 def test_prompt_evidence_is_kept_out_of_populated_sample_records(self):
  self.adapter("""
start();phase='gui';callbacks.pre_gui(gui);callbacks.post_gui(gui)
tick=tick+0.21;phase='frame';callbacks.frame()
local found=false
for _,r in ipairs(records) do
 if r.kind=='sample' then
  assert(not (r.prompt and r.prompt.evidence),'prompt detail must not overflow combined sample')
  assert(not (r.camera.prompt and r.camera.prompt.evidence))
 end
 if r.kind=='prompt' and r.prompt.evidence then found=true end
end
assert(found,'separate bounded projection evidence is required')
""",SETUP)
