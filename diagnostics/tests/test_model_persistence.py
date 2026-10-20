import unittest
import test_head_integration as heads

class ModelPersistenceTests(unittest.TestCase):
    adapter = heads.HeadIntegrationTests.adapter
    head = heads.HeadIntegrationTests.head

    def test_healthy_third_person_has_no_visibility_setters(self):
        self.head('''
start();assert(head.visible)
local writes=#model_writes;local materials=#head.material_writes
for i=1,100 do step();assert(head.visible) end
assert(#model_writes==writes,'healthy OTS must not toggle DrawSelf')
assert(#head.material_writes==materials,'healthy OTS must not toggle materials')
step(function(s) s.aim.raw=1 end);assert(not head.visible and near(position[3],3))
''')

    def test_missing_camera_callback_is_hidden_before_render(self):
        self.head('''
start();callbacks.LockScene=nil;step()
assert(not head.visible and near(position[3],3),'rendered FP must hide the head')
assert(ui():find('WAIT_RENDER_CAMERA',1,true))
''')

    def test_render_rechecks_menu_freshness_and_owned_camera(self):
        for mutation in ['menu_open=true', 'tick=tick+1']:
            self.head('''
start();'''+mutation+'''
phase='render';callbacks.BeginRendering();phase='frame'
assert(not head.visible and near(position[3],3))
''')
        self.head('''
start();position[3]=3;phase='render';callbacks.BeginRendering();phase='frame'
assert(not head.visible and near(position[3],3))
''')

    def test_unavailable_render_guard_uses_conservative_lifecycle(self):
        self.head('''
start();local writes=#model_writes;step();assert(head.visible and #model_writes>writes)
''','''
local register=re.on_pre_application_entry
re.on_pre_application_entry=function(n,f) if n=='BeginRendering' then return false end;return register(n,f) end
''')

    def test_direct_transport_integrates_with_camera_without_status_file(self):
        self.head('''
start();assert(head.visible and near(position[3],4))
assert(ui():find('NATIVE_DIRECT',1,true))
step(function(s) s.aim.raw=1 end);assert(not head.visible and near(position[3],3))
''','''
re7_ots_native={api_version=1,read=function() return 'native' end,clock=function() return tick end}
json.load_string=function(s) assert(s=='native');return fixture end
local load=json.load_file
json.load_file=function(p) assert(not p:find('status.json',1,true),'status file forbidden');return load(p) end
os.clock=function() return -999 end
''')

    def test_native_clock_failure_cleans_up_without_waiting_for_poll(self):
        self.head("""
start();assert(head.visible);broken_clock=true;step()
assert(not head.visible and near(position[3],3))
""","""
local broken_clock=false
re7_ots_native={api_version=1,read=function() return 'native' end,
 clock=function() if broken_clock then error('clock') end;return tick end}
json.load_string=function() return fixture end
""")

    def test_two_heads_keep_materials_without_writes_and_external_change_latches(self):
        self.head("""
start();assert(head.visible and second.visible)
local draw=#model_writes;local mats=#head.material_writes+#second.material_writes
for i=1,100 do step() end
assert(#model_writes==draw and #head.material_writes+#second.material_writes==mats)
second.material_enabled[second.materials[1]]=false;step()
assert(ui():find('OWNERSHIP_LOST',1,true))
assert(not head.visible and not second.visible)
""","""
local second=node(head_path);attach(rig,second)
for i=1,60 do attach(player,node()) end
model_hook=nil
""")
