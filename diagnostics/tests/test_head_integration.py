import unittest
import test_explore_lua as camera_tests
from test_ethan_model import HARNESS
from test_rt_models import MIA


class HeadIntegrationTests(unittest.TestCase):
    adapter = camera_tests.ExploreAdapterTests.adapter
    def head(self, tail, before=''):
        self.adapter(tail, HARNESS + '''
config.ethan_head=true
local lookup=sdk.get_managed_singleton
sdk.get_managed_singleton=function(n)
 assert(phase=='update' or phase=='gui' or phase=='lock' or phase=='reset' or phase=='render','model SDK outside callback')
 return lookup(n)
end
local camera_set=tr.set_position
tr.set_position=function(self,p,no_dirty)
 if p.z==3 and (phase=='reset' or phase=='render' or fixture and type(fixture.aim)=='table' and fixture.aim.raw==1) then assert(not head.visible,'hide head BEFORE actual FP cleanup') end
 return camera_set(self,p,no_dirty)
end
model_hook=function(n,v)
 assert(n==head,'only local head can change')
 if v then assert(position[3]>3.5,'show follows verified camera offset') end
end
''' + before)

    def test_model_shows_only_after_clearance_and_hides_for_all_fallbacks(self):
        for mutation in ['s.aim.raw=1', 's.inventory.raw=1', 's.foreground=false',
                         "s.interact.slot_hex='0x1';s.interact.ct_low16_nonzero=true;s.interact.ct_low32_nonzero=true"]:
            with self.subTest(mutation=mutation):
                self.head("start();assert(head.visible);step(function(s) " + mutation + ''' end)
assert(not head.visible and near(position[3],3));assert(player.visible and npc.visible)
for i=1,12 do step() end;assert(head.visible)
key=true;step();key=false;assert(not head.visible and near(position[3],3))
''')

    def test_blend_and_near_camera_do_not_expose_head(self):
        self.head("start();assert(not head.visible);assert(near(position[3],3.2))",
                  "config.distance=0.2;config.shoulder=0;config.height=0")

    def test_early_camera_keeps_head_ordering_and_first_person_cleanup(self):
        self.head('''
start();assert(head.visible and near(gui_projection[3],4))
step(function(s) s.aim.raw=1 end);assert(not head.visible and near(position[3],3))
for i=1,12 do step() end;assert(head.visible)
phase='reset';callbacks.reset();assert(not head.visible and near(position[3],3))
''', 'config.early_camera=true')

    def test_menu_stale_invalid_and_reset_hide(self):
        self.head('''
start();menu_open=true;step();assert(not head.visible)
menu_open=false;for i=1,12 do step() end;assert(head.visible)
fail_json=true;step();assert(not head.visible and near(position[3],3))
fail_json=false;for i=1,12 do step() end;assert(head.visible)
tick=tick+1;phase='lock';callbacks.LockScene();phase='frame'
assert(not head.visible and near(position[3],3))
for i=1,12 do step() end;phase='reset';callbacks.reset()
assert(not head.visible and near(position[3],3));assert(saved.ethan_head==true and saved.enabled==nil)
''')

    def test_model_failure_does_not_prevent_camera_cleanup(self):
        self.head('''
start();head.throw=true;key=true;step();key=false
assert(not head.visible and near(position[3],3));assert(ui():find('WRITE_FAILED',1,true))
''')
        self.head('''
start();head.ignore=true;key=true;step();key=false
assert(near(position[3],3),'camera must restore even if head cannot hide')
assert(ui():find('WRITE_FAILED',1,true))
''', "tr.set_position=camera_set -- deliberately allow a failed head hide")

    def test_new_generation_reacquires_and_old_target_is_never_restored(self):
        self.head('''
start();head.visible=false;step();assert(ui():find('OWNERSHIP_LOST',1,true))
for i=1,4 do step() end;assert(not head.visible,'same epoch stays latched')
for i=1,12 do step(function(s) s.player.generation=2 end) end
assert(head.visible,'new generation re-acquires')
''')

    def test_disabled_model_has_zero_sdk_reads(self):
        self.head("start();assert(model_reads==0 and #model_writes==0)",
                  "config.ethan_head=false;tr.set_position=camera_set;model_hook=nil")

    def test_malformed_player_keeps_owned_head_cleanup_available(self):
        for value in ['17', 'true']:
            with self.subTest(value=value):
                self.head("""start();step(function(s) s.player="""+value+""" end)
assert(not head.visible and near(position[3],3))
phase='reset';callbacks.reset();assert(not head.visible and near(position[3],3))
""")

    def test_no_acquisition_from_stale_or_unsupported_native(self):
        self.head("""
for i=1,10 do step(function(s) s.build='OTHER' end) end
assert(model_reads==0,'unsupported native build must not authorize model writes')
""", "tr.set_position=camera_set;model_hook=nil")
        self.head("""
for i=1,10 do step() end
assert(model_reads==0)
ui_head=true;ui();tick=tick+1;phase='update';callbacks.UpdateBehavior();phase='frame'
assert(model_reads==0,'stale snapshot must not acquire a new head')
""", "config.ethan_head=false;tr.set_position=camera_set;model_hook=nil")

    def test_strict_opt_in_and_disable_restore_baseline(self):
        self.head("start();assert(model_reads==0)",
                  "config.ethan_head='true';tr.set_position=camera_set;model_hook=nil")
        self.head('''
start();ui_head=false;ui();step();assert(head.visible,'explicit opt-out restores original DrawSelf')
assert(saved.ethan_head==false)
''', "tr.set_position=camera_set;model_hook=nil")

    def test_full_pack_mia_hides_both_parts_before_aim_camera_restore(self):
        self.adapter("""
start();assert(head.visible and hair.visible)
step(function(s) s.aim.raw=1 end)
assert(not head.visible and not hair.visible and near(position[3],3))
for i=1,12 do step() end;assert(head.visible and hair.visible)
hair.throw=true;key=true;step();key=false
assert(near(position[3],3),'part failure must not block camera restoration')
assert(not head.visible and not hair.visible)
""",HARNESS+MIA+"""
config.ethan_head=true;config.full_model_pack=true
local camera_set=tr.set_position
tr.set_position=function(self,p,dirty)
 if p.z==3 and fixture and fixture.aim.raw==1 then assert(not head.visible and not hair.visible,'hide BOTH parts first') end
 return camera_set(self,p,dirty)
end
""")

    def test_collision_hides_head_before_near_or_failed_camera_fallback(self):
        physics=(camera_tests.ROOT/'tests/physics_harness.lua').read_text().replace('local physics=Physics.new(sdk_physics)','')
        setup="local collision_wall,collision_fail;do\n"+physics+"""
for k,v in pairs(sdk_physics) do if k~='get_managed_singleton' then sdk[k]=v end end
collision_wall=function(d) layers[0]=d and {hit(1,3,3+d,wall)} or nil end
collision_fail=function() cast_error=true end
end
config.collision=true;config.shoulder=0;config.height=0
"""
        self.head("""
start();assert(head.visible)
collision_wall(0.5);step();assert(not head.visible and near(position[3],3.38))
collision_wall(0.05);step();assert(not head.visible and near(position[3],3))
collision_fail();step();assert(not head.visible and near(position[3],3))
assert(ui():find('WAIT_COLLISION',1,true))
""",setup)
