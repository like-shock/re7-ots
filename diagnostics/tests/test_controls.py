import unittest
import test_explore_lua as camera_tests

class TransitionCoreTests(unittest.TestCase):
    core=camera_tests.ExploreCoreTests.core

    def test_aim_release_hold_and_repeated_aim_never_relax_warmup(self):
        self.core('''
local cfg=Core.settings({schema=1,aim_release_delay=1})
local g=Core.new();warm(g,0)
local s=snapshot(10);s.aim.raw=1;g:ingest(s,1)
for i=11,17 do g:ingest(snapshot(i),i/10) end
local ok,why=g:ready(1.7,cfg);assert(not ok and why=='WAIT_AIM_RELEASE')
s=snapshot(18);s.aim.raw=1;g:ingest(s,1.8)
for i=19,27 do g:ingest(snapshot(i),i/10) end
assert(not g:ready(2.7,cfg));g:ingest(snapshot(28),2.8);assert(g:ready(2.8,cfg))
s=snapshot(29);s.aim.raw=1;g:ingest(s,2.9)
g:ingest(snapshot(30),3);assert(not g:ready(3,{schema=1,aim_release_delay=0}))
''')

    def test_new_configuration_is_strict_finite_and_bounded(self):
        self.core('''
local c=Core.settings({schema=1,aim_release_delay=99,entry_blend=-1,collision='true',collision_clearance=0/0})
assert(c.aim_release_delay==1.5 and c.entry_blend==0.05 and c.collision==false and c.collision_clearance==0.12)
c=Core.settings({schema=1,aim_release_delay=0/0,entry_blend='fast'})
assert(c.aim_release_delay==0.5 and c.entry_blend==0.15)
''')

class TransitionAdapterTests(unittest.TestCase):
    adapter=camera_tests.ExploreAdapterTests.adapter

    def test_f7_is_single_edge_and_persists_only_when_enabled_fresh_foreground(self):
        self.adapter('''
start();shoulder_key=true;step();assert(near(position[1],0.75));assert(saved.shoulder==-0.25)
for i=1,5 do step() end;assert(near(position[1],0.75),'held key must not oscillate')
shoulder_key=false;step();shoulder_key=true;step();assert(near(position[1],1.25))
''')
        for setup in ['menu_open=true','fixture.foreground=false','fail_json=true','tick=tick+1','key=true;step();key=false']:
            with self.subTest(setup=setup):
                self.adapter('start();'+setup+''';tick=tick+0.1;shoulder_key=true;phase='frame';callbacks.frame()
phase='reset';callbacks.reset();assert(saved.shoulder==0.25,'ignored F7 must not persist a swap')
''')
        self.adapter('''
for i=1,12 do step() end;shoulder_key=true;step();phase='reset';callbacks.reset()
assert(saved.shoulder==0.25,'disabled camera does not consume shoulder shortcut')
''')

    def test_aim_hold_and_entry_blend_are_used_by_actual_adapter(self):
        self.adapter('''
start();assert(position[3]>3 and position[3]<4)
for i=1,8 do step() end;assert(near(position[3],4))
step(function(s) s.aim.raw=1 end);assert(near(position[3],3))
for i=1,9 do step() end;assert(near(position[3],3));assert(ui():find('WAIT_AIM_RELEASE',1,true))
for i=1,18 do step() end;assert(near(position[3],4))
''', 'config.aim_release_delay=1.2;config.entry_blend=0.8')

    def test_reset_offsets_preserves_feature_and_transition_settings(self):
        self.adapter('''
imgui.button=function(label) return label=='Reset offsets' end
ui();assert(saved.distance==0.9 and saved.shoulder==0.25 and saved.height==0.15)
assert(saved.ethan_head and saved.full_model_pack and saved.collision)
assert(saved.collision_clearance==0.2 and saved.aim_release_delay==1.1 and saved.entry_blend==0.7)
''','''
config.ethan_head=true;config.full_model_pack=true;config.collision=true
config.collision_clearance=0.2;config.aim_release_delay=1.1;config.entry_blend=0.7
''')
