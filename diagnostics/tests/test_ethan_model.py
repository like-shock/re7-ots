from pathlib import Path
import unittest
from lua_runner import run_lua

ROOT = Path(__file__).resolve().parents[1]
HARNESS = (ROOT/'tests/model_backend_harness.lua').read_text()


def module(name, symbol):
    p = ROOT / ('lua/' + name + '.lua')
    assert p.exists(), f'{name} not implemented'
    return f'local {symbol}=(function()\n{p.read_text()}\nend)()\n'


class EthanBackendTests(unittest.TestCase):
    def check(self, scenario):
        run_lua(module('model/ethan', 'Ethan') + HARNESS + '''
local b=Ethan.new(sdk);b:set_epoch('native:1')
''' + scenario)

    def test_indexed_sdk_array_does_not_require_lua_table_elements(self):
        self.check('''
for _,n in ipairs({rig,head}) do
 n.go.methods.get_Components=function() return {
  get_elements=function() error('do not materialize the C++ vector') end,
  get_size=function() return #n.components end,
  get_element=function(_,i) assert(i>=0 and i<#n.components);return n.components[i+1] end} end
end
local s=b:read();assert(s,'use indexed SystemArray API, including zero-length arrays')
assert(b:write(s.id,true,false));assert(not head.visible and player.visible and npc.visible)
''')

    def test_invalid_or_oversized_component_array_stops_before_element_read(self):
        for count in ['-1','0/0','1.5','65','"1"']:
            self.check('''
rig.go.methods.get_Components=function() return {get_size=function() return '''+count+''' end,
 get_element=function() error('must validate count first') end} end
assert(not b:read());assert(b.note~='WAIT_MODEL_API','count validation precedes reads')
assert(#model_writes==0)
''')

    def test_nil_array_and_holes_cannot_hide_a_duplicate(self):
        self.check('''
rig.go.methods.get_Components=function() return nil end;assert(not b:read())
rig.go.methods.get_Components=function() return {get_size=function() return 1 end,
 get_element=function() return nil end} end
assert(not b:read());assert(#model_writes==0)
''')

    def test_sixty_four_components_are_supported(self):
        self.check('''
for i=2,64 do head.components[i]=managed('via.Component',{}) end
assert(b:read());head.components[65]=managed('via.Component',{});assert(not b:read())
''')

    def test_only_exact_local_head_and_compare_before_write(self):
        self.check('''
local s=b:read();assert(s and s.visible)
assert(b:write(s.id,true,false));assert(not head.visible)
assert(player.visible and npc.visible and #model_writes==1)
assert(not b:write(s.id,true,false));assert(#model_writes==1,'compare before write')
local p=b:head_position(s.id);assert(p[1]==1 and p[2]==3 and p[3]==3)
b:set_epoch('native:2');assert(not b:write(s.id,false,true));assert(not head.visible)
''')

    def test_wrong_path_materials_body_and_missing_methods_block(self):
        for change in ["head.path='other/'..head_path", "head.path=head_path..'.999'",
                       "head.materials[1]='pl0000_body'", "head.materials[5]=head.materials[1]",
                       "head.go.missing.set_DrawSelf=true", "head.tr.missing.getJointByName=true",
                       "head.joint_owner=player.tr", "head.components[2]=node(head_path).mesh",
                       "head.resource.missing['ToString()']=true"]:
            with self.subTest(change=change):
                self.check(change+";assert(not b:read());assert(#model_writes==0)")

    def test_resource_prefix_and_format_suffix(self):
        self.check("head.path='NATIVES\\\\STM\\\\CHARACTER\\\\PLAYER\\\\PL0000\\\\PL0002\\\\PL0002.MESH.220128762';assert(b:read())")

    def test_duplicates_reparenting_and_replacement_never_receive_stale_write(self):
        self.check('''
local s=b:read();attach(rig,node(head_path));assert(not b:read())
assert(not b:write(s.id,true,false));assert(#model_writes==0)
''')
        self.check('''
local s=b:read();rig.children={};attach(npc,head)
assert(not b:write(s.id,true,false));assert(head.visible and #model_writes==0)
''')
        self.check('''
local s=b:read();local old=head;rig.children={};head=node(head_path);attach(rig,head)
assert(not b:write(s.id,true,false));assert(head.visible and old.visible and #model_writes==0)
''')

    def test_cycle_depth_and_node_budget_fail_closed(self):
        for change in ["head.next_override=head", "local n=rig;for i=1,14 do local c=node();attach(n,c);n=c end",
                       "for i=1,130 do attach(player,node()) end", "head.parent=npc"]:
            with self.subTest(change=change):
                self.check(change+";assert(not b:read());assert(#model_writes==0)")

    def test_no_epoch_no_player_and_invalid_head_position(self):
        self.check("b:set_epoch(nil);assert(not b:read());assert(model_reads==0)")
        self.check("player=nil;assert(not b:read())")
        self.check("local s=b:read();head.position[1]=0/0;assert(not b:head_position(s.id))")

    def test_cached_reads_skip_unrelated_components_but_each_write_rescans(self):
        self.check("""
for i=1,60 do attach(player,node()) end
local s=b:read();local count=model_component_reads
assert(b:read_cached().id==s.id);assert(b:read_cached().id==s.id);assert(b:head_position(s.id))
assert(model_component_reads-count<=3,'cached reads must only inspect the selected mesh')
count=model_component_reads
assert(b:write(s.id,true,false));assert(model_component_reads-count>=62,'writes require a complete rescan')
local cached=b:read_cached();attach(rig,node(head_path))
assert(not b:write(cached.id,false,true));assert(not head.visible,'new duplicate blocks cached target write')
""")

    def test_cached_routes_revalidate_reorder_reparent_signature_and_epoch(self):
        self.check("""
local s=b:read();table.insert(rig.children,1,node());rig.children[1].parent=rig
assert(b:read_cached().id==s.id,'reordered path must be reacquired')
head.materials[1]='wrong';assert(not b:read_cached());assert(#model_writes==0)
""")
        self.check("""
local s=b:read();rig.children={};attach(npc,head)
assert(not b:read_cached());assert(not b:write(s.id,true,false))
""")
        self.check("""
local s=b:read();local old=head;rig.children={};head=node(head_path);attach(rig,head)
local replacement=b:read_cached();assert(replacement.id~=s.id)
assert(not b:write(s.id,true,false));assert(head.visible and old.visible)
b:set_epoch('native:2');assert(b:read_cached().id~=replacement.id)
""")
