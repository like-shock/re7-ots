import unittest
import test_rt_models as models


class ModelPerformanceTests(unittest.TestCase):
    check = models.RTModelTests.check

    def test_steady_camera_frames_do_not_scan_the_tree_per_material(self):
        for copies in [2, 4]:
            with self.subTest(copies=copies):
                self.check('''
local copies={head}
for i=2,'''+str(copies)+''' do local n=node(head_path);attach(rig,n);copies[i]=n end
for i=1,60 do attach(player,node()) end
g:configure(true,'s:1',true);assert(g:before_restore());assert(g:after_apply({1,3,5}))
local scans=g:status().search.full_scans;local reads=model_component_reads
for frame=1,100 do
 -- UpdateBehavior restores FP; the camera callback checks hiding again.
 assert(g:before_restore());assert(g:before_restore())
 for _,n in ipairs(copies) do
  assert(not n.visible)
  for _,name in ipairs(n.materials) do assert(n.material_enabled[name]==false) end
 end
 assert(g:after_apply({1,3,5}))
 for _,n in ipairs(copies) do
  assert(n.visible)
  for _,name in ipairs(n.materials) do assert(n.material_enabled[name]==true) end
 end
end
local count=g:status().search.full_scans-scans
assert(count<=300,'whole-tree scans scale with material writes: '..count..'/100 frames')
assert(model_component_reads-reads<=40000,'unchanged branches repeatedly enumerated')
assert(#player.material_writes==0 and #npc.material_writes==0)
assert(g:before_restore())
scans=g:status().search.full_scans;reads=model_component_reads
local writes=#model_writes
for frame=1,100 do assert(g:before_restore());assert(g:before_restore()) end
assert(g:status().search.full_scans==scans,'steady FP needs no write authorization scan')
assert(model_component_reads-reads<=5000 and #model_writes==writes)
''')

    def test_last_show_setter_cannot_leave_an_unverified_group_visible(self):
        self.check('''
g:configure(true,'s:1',true);assert(g:before_restore())
local added
head.material_hook=function(name,value)
 if value and name=='pl0050_eye_shadow' and not added then
  added=node(head_path);attach(rig,added)
 end
end
assert(not g:after_apply({1,3,5}),'check the complete group after the final show setter')
assert(added and #added.material_writes==0 and added.visible,'never acquire a new copy mid-operation')
assert(not head.visible)
for _,name in ipairs(head.materials) do assert(head.material_enabled[name]==false) end
assert(g:status().state=='TARGET_CHANGED')
''')

    def test_material_order_and_signature_are_rechecked_during_group_writes(self):
        self.check('''
g:configure(true,'s:1',true);assert(g:before_restore())
model_hook=function(n,v)
 if n==head and v then head.materials[1],head.materials[2]=head.materials[2],head.materials[1] end
end
assert(g:after_apply({1,3,5}))
for _,name in ipairs(head.materials) do assert(head.material_enabled[name]==true) end
assert(g:before_restore())
model_hook=function(n,v) if n==head and v then head.materials[1]='body' end end
local writes=#head.material_writes
assert(not g:after_apply({1,3,5}))
assert(#head.material_writes==writes,'changed signature cannot receive cached material writes')
''')

    def test_exception_does_not_authorize_a_later_callback_or_generation(self):
        self.check('''
g:configure(true,'s:1',true);assert(g:before_restore())
head.throw_material=true
assert(not g:after_apply({1,3,5}));head.throw_material=false
local replacement=node(head_path);rig.children={};attach(rig,replacement)
g:configure(false,'s:1',true);assert(not g:before_restore())
assert(#replacement.material_writes==0 and replacement.visible)
g:configure(true,'s:2',true);assert(g:before_restore());assert(not replacement.visible)
assert(g:after_apply({1,3,5}));assert(replacement.visible)
''')

    def test_release_revalidates_membership_before_each_restore(self):
        for change in ["g:configure(false,'s:1',true)", "g:configure(true,'s:1',false)"]:
            with self.subTest(change=change):
                self.check('''
g:configure(true,'s:1',true);assert(g:before_restore())
local added
head.material_hook=function(name,value)
 if value and not added then added=node(head_path);attach(rig,added) end
end
'''+change+'''
assert(not g:before_restore(),'changed group cannot be released as DISABLED')
assert(g:status().state~='DISABLED' and #g.parts==1)
assert(#added.material_writes==0 and added.visible)
assert(head.material_enabled.pl0050_eye==false,'later release writes must be rejected')
-- Re-enabling cannot make a closed controller's failed release use a cache.
g:configure(true,'s:1',true);assert(not g:before_restore())
assert(head.material_enabled.pl0050_eye==false and #added.material_writes==0)
''')

    def test_failed_show_invalidates_group_before_later_channels_and_cleanup(self):
        self.check('''
g:configure(true,'s:1',true);assert(g:before_restore())
local added,at_fault
head.material_hook=function(name,value)
 if value and name=='pl0050_face' and not added then
  head.material_enabled[name]=true
  added=node(head_path);added.mesh=head.mesh;added.components={head.mesh};attach(rig,added)
  at_fault=#head.material_writes
  error('setter changed membership then threw')
 end
end
assert(not g:after_apply({1,3,5}))
assert(#head.material_writes==at_fault,'failed setter must not authorize writes to a now-shared mesh')
assert(#added.material_writes==0)
''')
