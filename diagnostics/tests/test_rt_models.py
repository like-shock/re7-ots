from pathlib import Path
import unittest
from lua_runner import run_lua
from test_ethan_model import module, HARNESS

ROOT=Path(__file__).resolve().parents[1]
GROUP=ROOT/'lua/model/group.lua'
MIA="""
head.path='character/player/pl2100/pl2102/pl2102.mesh'
head.materials={'em2050_Skin_Evil','em2050_Hair','eye_Tear','em2050_Skin_Mia','em2050_Skin_Blend','em2050_Eye_Blend','Eye_Shadow'}
local hair=node('character/player/pl2100/pl2100/pl2120.mesh')
hair.materials={'em2070_Hair2','em2070_Hair3'};attach(rig,hair)
"""


class RTModelTests(unittest.TestCase):
    def check(self,scenario):
        self.assertTrue(GROUP.exists(),'RT model group not implemented')
        code=''.join(module(n,s) for n,s in [('explore/core','Core'),('model/visibility','Visibility'),
             ('model/ethan','Ethan'),('model/session','HeadSession'),('model/group','ModelGroup')])
        run_lua(code+HARNESS+"\nlocal clock=0;local g=ModelGroup.new(sdk,function() return clock end)\n"+scenario)

    def test_mia_parts_keep_separate_baselines_and_leave_body_alone(self):
        self.check(MIA+"""
hair.visible=false;g:configure(true,'s:1',true)
assert(g:before_restore());assert(not head.visible and not hair.visible)
assert(g:after_apply({1,3,5}));assert(head.visible and hair.visible)
g:configure(false,'s:1',true);assert(g:before_restore())
assert(head.visible and not hair.visible,'independent original visibility')
assert(player.visible and npc.visible);assert(g:status().state=='DISABLED')
""")

    def test_partial_group_show_cleans_up_every_owned_part(self):
        self.check(MIA+"""
g:configure(true,'s:1',true);assert(g:before_restore())
model_hook=function(n,v) if n==hair and v then n.visible=true;error('partial show') end end
assert(not g:after_apply({1,3,5}));assert(not head.visible and not hair.visible)
local writes=#model_writes;assert(not g:after_apply({1,3,5}));assert(#model_writes==writes)
""")

    def test_missing_companion_duplicates_mixed_families_and_wrong_materials_block(self):
        for change in ["rig.children={head}","attach(rig,node(head_path))",
                       "local dup=node(head.path);dup.materials=head.materials;attach(rig,dup)",
                       "hair.materials[1]='body'", "hair.parent=npc"]:
            with self.subTest(change=change):
                self.check(MIA+change+";g:configure(true,'s:1',true);assert(not g:before_restore());assert(#model_writes==0)")

    def test_replacement_and_epoch_do_not_restore_old_values_to_new_parts(self):
        self.check(MIA+"""
g:configure(true,'s:1',true);assert(g:before_restore());assert(g:after_apply({1,3,5}))
local old=head;head=node(old.path);head.materials=old.materials;rig.children={};attach(rig,head);attach(rig,hair)
g:configure(false,'s:1',true);assert(not g:before_restore());assert(head.visible)
g:configure(true,'s:2',true);assert(g:before_restore());assert(not head.visible and not hair.visible)
""")

    def test_legacy_ethan_mode_and_mode_change_release(self):
        self.check("""
g:configure(true,'s:1',false);assert(g:before_restore());assert(g:after_apply({1,3,5}))
g:configure(true,'s:1',true);assert(g:before_restore());assert(not head.visible)
assert(g:status().family=='ethan');assert(g:after_apply({1,3,5}))
g:configure(false,'s:1',true);assert(g:before_restore());assert(head.visible)
""")

    def test_all_verified_single_part_profiles(self):
        profiles=[
          ('ch8/character/player/pl1000/pl1002/pl1002.mesh',['em7350_face__Character_Default']),
          ('ch9/character/player/pl9000/pl9002/pl9002.mesh',['pl9050_face','pl9050_hair']),
          ('character/player/pl2000/pl2002/pl2002.mesh',['em2050_Skin_Evil','em2070_Hair2','em2050_Hair','em2070_Hair3','eye_Tear','em2050_Skin_Mia','em2050_Skin_Blend','em2050_Eye_Blend','Eye_Shadow']),
          ('character/player/pl3000/pl3002/pl3002.mesh',['pl3050_face__Character_Default','pl3050_eye__Character_Default','pl3050_Equipment__Character_Default','pl3050_hair__Character_AlphaTest']),
          ('character/player/pl3100/pl3102/pl3102.mesh',['pl3050_face__Character_Default','pl3050_eye__Character_Default','pl3050_Equipment__Character_Default','pl3050_hair__Character_AlphaTest']),
          ('character/player/pl3400/pl3402/pl3402.mesh',['em3450_eye','em3450_hair','em3450_Eye_Shadow','em3450_Eye_Tier','pl3450_face','pl3400_hair'])]
        for path,mats in profiles:
            with self.subTest(path=path):
                self.check("head.path="+repr(path)+";head.materials={"+','.join(repr(m) for m in mats)+"};"+"""
g:configure(true,'s:1',true);assert(g:before_restore());assert(not head.visible)
assert(g:after_apply({1,3,5}));assert(head.visible and player.visible and npc.visible)
""")

    def test_no_epoch_discovery_throttling_and_head_clearance(self):
        self.check("""
g:configure(true,nil,true);assert(not g:before_restore());assert(model_reads==0)
head.path='unknown';g:configure(true,'s:1',true);assert(not g:before_restore())
local count=model_reads;clock=0.1;assert(not g:before_restore());assert(model_reads==count)
head.path=head_path;clock=0.6;assert(g:before_restore())
assert(not g:after_apply({1,3,3.2}));assert(not head.visible)
assert(g:after_apply({1,3,5}));assert(head.visible)
g:configure(true,nil,true);g:before_restore();assert(not head.visible)
assert(not g:after_apply({1,3,5}));assert(not head.visible)
""")
