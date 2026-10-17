import unittest
import test_rt_models as models
import test_explore_lua as camera
import test_ethan_model as backend
import test_head_materials as materials


class EthanInstanceTests(unittest.TestCase):
    check = models.RTModelTests.check
    adapter = camera.ExploreAdapterTests.adapter

    def test_each_instance_hides_and_restores_its_own_baseline_in_both_modes(self):
        for full in ['true', 'false']:
            with self.subTest(full_pack=full):
                self.check('''
local second=node(head_path);attach(rig,second)
second.visible=false;second.material_enabled.pl0050_eye=false
g:configure(true,'s:1','''+full+''')
assert(g:before_restore(),'verified separate heads must both be acquired')
assert(#g.parts==2 and not head.visible and not second.visible)
for _,n in ipairs({head,second}) do for _,name in ipairs(n.materials) do
 assert(n.material_enabled[name]==false,'every verified head material hidden')
end end
assert(g:after_apply({1,3,5}))
assert(head.visible and not second.visible,'do not activate an originally disabled copy')
assert(head.material_enabled.pl0050_eye==true and second.material_enabled.pl0050_eye==false)
assert(g:status().state=='RESTORED_BASELINE' and g:status().instances==2)
g:configure(false,'s:1','''+full+''');assert(g:before_restore())
assert(head.visible and not second.visible and second.material_enabled.pl0050_eye==false)
assert(#player.material_writes==0 and #npc.material_writes==0 and player.visible and npc.visible)
''')

    def test_sibling_reordering_does_not_exchange_instance_baselines(self):
        self.check('''
local second=node(head_path);attach(rig,second);second.material_enabled.pl0050_eye=false
g:configure(true,'s:1',true);assert(g:before_restore())
rig.children={second,head}
assert(g:after_apply({1,3,5}),'identities must not depend on discovery order')
assert(head.material_enabled.pl0050_eye==true and second.material_enabled.pl0050_eye==false)
''')

    def test_new_or_replaced_copy_blocks_show_and_never_receives_stale_values(self):
        for mutation in ['attach(rig,new)', 'rig.children={head};attach(rig,new)']:
            with self.subTest(mutation=mutation):
                self.check('''
local second=node(head_path);attach(rig,second)
g:configure(true,'s:1',true);assert(g:before_restore());assert(g:after_apply({1,3,5}))
local new=node(head_path);'''+mutation+'''
assert(not g:after_apply({1,3,5}))
assert(not head.visible and #new.material_writes==0 and new.visible)
g:configure(true,'s:2',true);assert(g:before_restore(),'new epoch must reacquire the current set')
assert(not new.visible)
''')

    def test_clearance_of_every_copy_is_required(self):
        self.check('''
local second=node(head_path);attach(rig,second);second.position={1,3,4.8}
g:configure(true,'s:1',true);assert(g:before_restore())
assert(not g:after_apply({1,3,5}))
assert(not head.visible and not second.visible)
for _,n in ipairs({head,second}) do for _,name in ipairs(n.materials) do
 assert(n.material_enabled[name]==false)
end end
''')

    def test_shared_objects_and_more_than_four_copies_still_block_before_writes(self):
        for mutation in ["head.components[2]=node('other/body.mesh').mesh",
                         "local second=node(head_path);second.components={head.mesh};attach(rig,second)",
                         "for i=1,4 do attach(rig,node(head_path)) end"]:
            self.check(mutation+'''
g:configure(true,'s:1',true);assert(not g:before_restore())
assert(#model_writes==0 and #head.material_writes==0)
''')

    def test_camera_fallback_hides_all_copies_even_after_one_material_fails(self):
        self.adapter('''
start();assert(head.visible and second.visible)
step(function(s) s.aim.raw=1 end)
assert(near(position[3],3))
for _,n in ipairs({head,second}) do for _,name in ipairs(n.materials) do
 assert(n.material_enabled[name]==false)
end end
for i=1,15 do step() end
second.throw_material=true;key=true;step();key=false
assert(near(position[3],3))
for _,n in ipairs({head,second}) do for _,name in ipairs(n.materials) do
 assert(n.material_enabled[name]==false)
end end
''', backend.HARNESS+'''
local second=node(head_path);attach(rig,second)
config.ethan_head=true;config.full_model_pack=true
local camera_set=tr.set_position
tr.set_position=function(self,p,dirty)
 if p.z==3 and (key or fixture and fixture.aim.raw==1) then for _,n in ipairs({head,second}) do
  assert(not n.visible,'hide every copy before FP camera restore')
  for _,name in ipairs(n.materials) do assert(n.material_enabled[name]==false) end
 end end
 return camera_set(self,p,dirty)
end
''')

    def test_four_instances_and_failure_details_fit_automatic_journal(self):
        class Capture(materials.FullPackMaterialTests):
            def adapter(self, tail, before=''):
                self.before = before
        fixture = Capture()
        fixture.test_full_pack_material_diagnostics_fit_the_actual_journal()
        self.adapter('''
start();for i=1,15 do step() end
for _,n in ipairs(copies) do for i=2,64 do n.components[i]=n.mesh end end
for i=1,15 do step() end
assert(ui():find('Automatic play log: RECORDING',1,true),'oversized record: '..max_line)
assert(max_line<=4096 and part_seen==4 and binding_seen and phase_seen,'retain each instance and failure detail')
''', fixture.before+'''
rig.children={head};head.path=head_path;head.materials=node(head_path).materials
local copies={head};for i=1,3 do local n=node(head_path);n.position={table.unpack(p)}
 attach(rig,n);copies[#copies+1]=n end
for i,n in ipairs(copies) do n.go.id=140000000000000+i*10;n.mesh.id=n.go.id+1 end
local part_seen,binding_seen=0,false
local seen={};local serialize=json.dump_string
json.dump_string=function(r)
 if r.kind=='model_part' then for name,part in pairs(r.model.parts) do
  assert(part.target and part.materials.pl0050_face,'identity and materials retained')
  if not seen[name] then seen[name]=true;part_seen=part_seen+1 end
 end end
 if r.kind=='model_binding' and r.model.diagnostic then binding_seen=true end
 return serialize(r)
end
''')
