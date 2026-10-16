import unittest
import test_ethan_model as backend
import test_rt_models as models
import test_explore_lua as camera
import test_head_materials as materials


class BindingDiagnosticTests(unittest.TestCase):
    check = backend.EthanBackendTests.check

    def test_shared_object_meshes_are_distinguished_from_separate_heads(self):
        self.check('''
head.components[2]=node('other/body.mesh').mesh
assert(not b:read());local d=b.diagnostic
assert(d and d.kind=='MULTIPLE_MESH_COMPONENTS')
assert(d.mesh_count==2 and d.unique_mesh_count==2 and #d.meshes==2)
assert(d.meshes[1].profile=='ethan' and d.meshes[2].profile=='OTHER')
assert(#model_writes==0 and #head.material_writes==0)
''')
        self.check('''
attach(rig,node(head_path));assert(not b:read());local d=b.diagnostic
assert(d and d.kind=='MULTIPLE_HEAD_OBJECTS' and #d.targets==2)
assert(d.targets[1].owner~=d.targets[2].owner)
assert(#model_writes==0)
''')

    def test_repeated_component_reference_and_bounded_evidence(self):
        self.check('''
for i=2,64 do head.components[i]=head.mesh end
assert(not b:read());local d=b.diagnostic
assert(d and d.kind=='MULTIPLE_MESH_COMPONENTS')
assert(d.mesh_count==64 and d.unique_mesh_count==1)
assert(#d.meshes==4 and d.truncated==true)
assert(#model_writes==0)
''')

    def test_component_array_failure_is_explained_and_recovery_clears_old_evidence(self):
        self.check('''
local read=rig.go.methods.get_Components
rig.go.methods.get_Components=function() return nil end
assert(not b:read());assert(b.diagnostic.kind=='COMPONENT_ARRAY_UNAVAILABLE')
rig.go.methods.get_Components=read
assert(b:read());assert(b.diagnostic==nil,'resolved target cannot retain an obsolete failure')
''')


class BindingDiagnosticGroupTests(unittest.TestCase):
    check = models.RTModelTests.check
    adapter = camera.ExploreAdapterTests.adapter

    def test_acquired_mia_diagnostics_cannot_stop_the_journal(self):
        # Reuse the populated material/phase/hand fixture, then introduce the
        # ambiguity after both parts have acquired their material baselines.
        class Capture(materials.FullPackMaterialTests):
            def adapter(self, tail, before=''):
                self.before = before
        fixture = Capture()
        fixture.test_full_pack_material_diagnostics_fit_the_actual_journal()
        self.adapter('''
start();for i=1,15 do step() end
for i=2,64 do head.components[i]=head.mesh;hair.components[i]=hair.mesh end
for i=1,15 do step() end
assert(ui():find('Automatic play log: RECORDING',1,true),'oversized record: '..max_line)
assert(max_line<=4096 and binding_seen and phase_seen and detail_seen)
''', fixture.before+'''
head.go.id=140000000000001;head.mesh.id=140000000000002
hair.go.id=140000000000003;hair.mesh.id=140000000000004
local binding_seen=false
local serialize=json.dump_string
json.dump_string=function(r)
 if r.kind=='model_binding' then
  local m=r.model
  binding_seen=m.diagnostic and m.diagnostic.kind=='MULTIPLE_MESH_COMPONENTS'
   and m.parts.mia2100.mesh_count==64 and m.parts.mia2100_hair.mesh_count==64
 end
 return serialize(r)
end
''')

    def test_full_pack_and_ethan_mode_report_the_actual_binding_failure(self):
        for full in ['true','false']:
            with self.subTest(full_pack=full):
                self.check('''
attach(rig,node(head_path));g:configure(true,'s:1','''+full+''')
assert(not g:before_restore());local s=g:status(true)
assert(s.binding=='WAIT_MODEL_AMBIGUOUS','family name is not proof of a match')
assert(s.diagnostic_kind=='MULTIPLE_HEAD_OBJECTS')
local d=s.diagnostic or s.parts.ethan.diagnostic
assert(d and #d.targets==2 and d.kind=='MULTIPLE_HEAD_OBJECTS')
assert(#model_writes==0)
''')

    def test_ambiguity_detail_reaches_bounded_automatic_logs_without_writes(self):
        encoder=(backend.ROOT/'tests/json_encoder.lua').read_text()
        for full in ['true','false']:
            with self.subTest(full_pack=full):
                self.adapter('''
start();for i=1,15 do step() end
assert(ui():find('Automatic play log: RECORDING',1,true))
assert(ui():find('MULTIPLE_MESH_COMPONENTS',1,true))
assert(found and max_line<=4096,'capture detail without overflowing journal')
assert(#model_writes==0 and #head.material_writes==0)
''', backend.HARNESS+'''
config.ethan_head=true;config.full_model_pack='''+full+'''
for i=2,64 do head.components[i]=head.mesh end
head.go.id=9007199254740981;head.mesh.id=9007199254740982
local p={12.345678329467773,23.456789016723633,34.567890167236328}
position={table.unpack(p)};head.position={table.unpack(p)}
local old_snapshot=snapshot
snapshot=function(n) local s=old_snapshot(n);s.player.xyz={table.unpack(p)};return s end
tr.call=function(_,name)
 assert(name=='getJointByName');return {call=function() return {x=p[1],y=p[2],z=p[3]} end}
end
sdk.typeof=function(name) return name end
local target={get_type_definition=function() return {is_a=function() return true end} end,
 get_position=function() return {x=p[1],y=p[2],z=p[3]} end}
player.go.methods['getComponent(System.Type)']=function(_,name)
 return {call=function() return player.go end,get_field=function(_,field)
  if field=='HandIK' then return {get_size=function() return 2 end,
   get_element=function() return {get_field=function() return target end} end} end
  return false
 end}
end
local found,max_line=false,0
local encode=(function()
'''+encoder+'''
end)()
json.dump_string=function(r)
 local line=encode(r);max_line=math.max(max_line,#line+1)
 if r.kind=='model_binding' and r.model then
  local d=r.model.diagnostic or r.model.parts.ethan
  if d and d.kind=='MULTIPLE_MESH_COMPONENTS' then found=d.mesh_count==64 and #d.meshes==4 end
 end
 return line
end
''')
