import unittest
import test_rt_models as models
import test_explore_lua as camera
import test_head_materials as materials


# Simulate the documented Component.get_Chain API without creating a SystemArray.
# The production backend must independently verify type, owner, bounds and cycles.
CHAIN = '''
local function chain_for(n,extra)
 local list={n.tr}
 for _,c in ipairs(n.components) do list[#list+1]=c end
 for _,c in ipairs(extra or {}) do list[#list+1]=c end
 for i,c in ipairs(list) do
  local old_type=c.get_type_definition
  c.get_type_definition=function(self)
   local ty=old_type(self);local old_is=ty.is_a
   ty.is_a=function(self,name) return name=='via.Component' or old_is(self,name) end
   return ty
  end
  c.methods.get_GameObject=function() return n.go end
  c.methods.get_Chain=function() return list[i+1] or list[1] end
 end
 n.go.methods.get_Components=function() return {} end
 return list
end
chain_for(rig);chain_for(head)
'''


class ReacquisitionTests(unittest.TestCase):
    check = models.RTModelTests.check
    adapter = camera.ExploreAdapterTests.adapter

    def test_fresh_resource_holders_do_not_change_render_target_identity(self):
        self.check('''
local second=node(head_path);attach(rig,second)
for _,n in ipairs({head,second}) do
 n.mesh.methods.getMesh=function() return managed('via.Resource',n.resource.methods) end
end
second.material_enabled.pl0050_eye=false
g:configure(true,'s:1',true)
assert(g:before_restore(),'fresh holders must not strand verified heads at WAIT_MODEL_TARGET')
assert(not head.visible and not second.visible)
assert(g:after_apply({1,3,5}));assert(head.visible and second.visible)
assert(head.material_enabled.pl0050_eye and not second.material_enabled.pl0050_eye)
assert(g:before_restore());assert(not head.visible and not second.visible)
assert(#npc.material_writes==0 and #player.material_writes==0)
''')

    def test_reacquired_head_joint_uses_current_owner_and_position(self):
        self.check('''
head.tr.methods.getJointByName=function() return managed('via.Joint',head.joint.methods) end
g:configure(true,'s:1',true);assert(g:before_restore())
assert(g:after_apply({1,3,5}))
head.position={1,3,4.8};assert(not g:after_apply({1,3,5}));assert(not head.visible)
head.joint_owner=npc.tr
assert(not g:before_restore(),'foreign joint must still block')
''')

    def test_component_chain_fallback_hides_only_verified_local_head(self):
        self.check(CHAIN+'''
g:configure(true,'s:1',true);assert(g:before_restore(),'valid complete component chain must be usable')
assert(not head.visible and player.visible and npc.visible)
assert(g:after_apply({1,3,5}));assert(head.visible)
g:configure(false,'s:1',true);assert(g:before_restore());assert(head.visible)
''')
        self.check(CHAIN+'''
rig.tr.methods.get_Chain=function() return nil end
head.mesh.methods.get_Chain=function() return nil end
g:configure(true,'s:1',true);assert(g:before_restore(),'null-terminated chains are complete too')
assert(not head.visible and player.visible and npc.visible)
''')

    def test_chain_counts_repeated_types_and_cannot_conceal_shared_mesh(self):
        self.check(CHAIN+'''
local other=node('other/body.mesh').mesh
chain_for(head,{other})
g:configure(true,'s:1',true);assert(not g:before_restore())
assert(#model_writes==0 and #head.material_writes==0)
assert(g:status(true).diagnostic.kind=='MULTIPLE_MESH_COMPONENTS')
''')

    def test_invalid_foreign_and_nonroot_cycles_reject_before_writing(self):
        for change in [
            'head.mesh.methods.get_GameObject=function() return npc.go end',
            'head.mesh.methods.get_Chain=function() return head.mesh end',
            'head.mesh.methods.get_Chain=function() return false end',
            'head.mesh.missing.get_Chain=true',
        ]:
            with self.subTest(change=change):
                self.check(CHAIN+change+'''
g:configure(true,'s:1',true);assert(not g:before_restore())
assert(#model_writes==0 and #head.material_writes==0)
''')

    def test_chain_bound_and_valid_array_are_not_bypassed(self):
        self.check(CHAIN+'''
local extra={};for i=1,62 do extra[i]=managed('via.Component',{}) end
chain_for(head,extra)
g:configure(true,'s:1',true);assert(g:before_restore(),'64 components including Transform supported')
g:configure(false,'s:1',true);assert(g:before_restore())
extra[63]=managed('via.Component',{});chain_for(head,extra)
g:configure(true,'s:2',true);assert(not g:before_restore())
assert(g:status().state=='WAIT_MODEL_COMPONENT_LIMIT')
''')
        self.check(CHAIN+'''
head.go.methods.get_Components=function() return {get_size=function() return 65 end,
 get_element=function() error('must not read oversized array') end} end
g:configure(true,'s:1',true);assert(not g:before_restore())
assert(g:status().state=='WAIT_MODEL_COMPONENT_LIMIT' and #model_writes==0)
''')

    def test_same_resource_path_on_replaced_mesh_never_receives_old_baseline(self):
        self.check('''
g:configure(true,'s:1',true);assert(g:before_restore())
local replacement=node(head_path)
head.mesh=replacement.mesh;head.components={head.mesh}
assert(not g:after_apply({1,3,5}))
assert(#replacement.material_writes==0 and replacement.visible)
''')

    def test_four_replaced_instances_keep_mismatch_evidence_within_journal_limit(self):
        class Capture(materials.FullPackMaterialTests):
            def adapter(self, tail, before=''):
                self.before = before
        fixture = Capture()
        fixture.test_full_pack_material_diagnostics_fit_the_actual_journal()
        self.adapter('''
start();for i=1,15 do step() end
for _,n in ipairs(copies) do n.go.id=n.go.id+100 end
for i=1,15 do step() end
assert(ui():find('Automatic play log: RECORDING',1,true),'identity journal stopped: '..ui())
assert(max_line<=4096 and mismatch_seen,'retain mismatch evidence for replaced targets')
''', fixture.before+'''
rig.children={head};head.path=head_path;head.materials=node(head_path).materials
local copies={head};for i=1,3 do local n=node(head_path);n.position={table.unpack(p)}
 attach(rig,n);copies[#copies+1]=n end
for i,n in ipairs(copies) do
 n.go.id=140000000000000+i*10;n.mesh.id=n.go.id+1
 n.tr.id=n.go.id+2;n.joint.id=n.go.id+3;n.resource.id=n.go.id+4
 n.go.methods.get_Name=function() return i==1 and string.rep('가',27) or string.rep('"',80) end
end
local mismatch_seen=false;local serialize=json.dump_string
json.dump_string=function(r)
 if r.kind=='model_part' then for _,part in pairs(r.model.parts) do
  assert(not part.target.name or utf8.len(part.target.name),'invalid UTF-8 would stop the real JSON journal')
 end end
 if r.kind=='model_binding' then for _,d in pairs(r.model.parts or {}) do
  if d.kind=='TARGET_IDENTITY_MISMATCH' and d.expected and d.observed[1] then mismatch_seen=true end
 end end
 return serialize(r)
end
''')
