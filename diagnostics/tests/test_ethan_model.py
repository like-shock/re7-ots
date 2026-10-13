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
