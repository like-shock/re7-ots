import unittest
import test_explore_lua as camera


class CameraPhaseTests(unittest.TestCase):
    adapter = camera.ExploreAdapterTests.adapter

    def test_early_mode_applies_before_gui_and_never_adds_second_offset(self):
        self.adapter('''
start();assert(near(gui_projection[3],4),'HUD projection sees displaced camera')
for i=1,40 do step();assert(near(position[3],4));assert(near(gui_projection[3],4)) end
local n=set_calls;phase='gui';callbacks.PrerenderGUI();phase='lock';callbacks.LockScene()
assert(set_calls==n,'duplicate callbacks never offset twice')
''', 'config.early_camera=true')

    def test_default_keeps_late_application_and_collects_scalar_phase_evidence(self):
        self.adapter('''
start();assert(near(gui_projection[3],3));assert(near(position[3],4))
tick=tick+0.21;callbacks.frame();local row=records[#records]
assert(row.kind=='scene_detail' and row.lua_clock_s==tick);local d=row.phase_probe
assert(d and d.phases.gui_before and d.phases.lock_after and d.phases.render)
assert(d.phases.gui_before.transform[3]==3 and d.phases.lock_after.transform[3]==4)
assert(d.phases.render.transform[3]==4)
''')

    def test_missing_or_reordered_gui_restores_and_reports_wait(self):
        self.adapter('''
start();local early=callbacks.PrerenderGUI;callbacks.PrerenderGUI=nil
step();assert(near(position[3],3));assert(ui():find('WAIT_EARLY_CAMERA',1,true))
phase='gui';early();phase='lock';callbacks.LockScene();assert(near(position[3],3),'late callback cannot revive missed frame')
callbacks.PrerenderGUI=early;for i=1,10 do step() end;assert(near(position[3],4))
''', 'config.early_camera=true')

    def test_engine_change_between_gui_and_lock_is_not_clobbered(self):
        self.adapter('''
start();tick=tick+0.1;seq=seq+1;fixture=snapshot(seq);callbacks.frame()
phase='update';callbacks.UpdateBehavior();phase='gui';callbacks.PrerenderGUI()
position={1,3,3.4};phase='lock';callbacks.LockScene()
assert(near(position[3],3.4));assert(ui():find('FAULT',1,true))
for i=1,10 do step() end;assert(near(position[3],3.4),'ownership fault remains disabled')
''', 'config.early_camera=true')

    def test_camera_replacement_and_aim_after_early_application_restore_safely(self):
        self.adapter('''
start();tick=tick+0.1;seq=seq+1;fixture=snapshot(seq);callbacks.frame()
phase='update';callbacks.UpdateBehavior();phase='gui';callbacks.PrerenderGUI()
camera_id=30;transform_id=40;position={1,3,2.5};phase='lock';callbacks.LockScene()
assert(near(position[3],2.5));assert(ui():find('WAIT_CAMERA_CHANGED',1,true))
''', 'config.early_camera=true')
        self.adapter('''
start();tick=tick+0.1;seq=seq+1;fixture=snapshot(seq);callbacks.frame()
phase='update';callbacks.UpdateBehavior();phase='gui';callbacks.PrerenderGUI()
tick=tick+0.06;seq=seq+1;fixture=snapshot(seq);fixture.aim.raw=1
phase='frame';callbacks.frame();phase='lock';callbacks.LockScene()
assert(near(position[3],3));assert(ui():find('FIRST_PERSON_AIM',1,true))
''', 'config.early_camera=true')

    def test_probe_failures_cannot_latch_camera_and_config_is_boolean(self):
        self.adapter('''
start();assert(near(position[3],4));assert(not ui():find('FAULT',1,true))
phase='reset';callbacks.reset();assert(saved.early_camera==false)
''', 'config.early_camera="true";config.ethan_head=true;sdk.get_managed_singleton=function() error("unavailable") end')
