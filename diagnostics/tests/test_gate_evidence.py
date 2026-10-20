import unittest
import test_explore_lua as camera
import test_obstacle as obstacle


class GateEvidenceTests(unittest.TestCase):
    core = camera.ExploreCoreTests.core
    adapter = camera.ExploreAdapterTests.adapter
    exercise = obstacle.ObstacleAdapterTests.exercise

    def test_native_gap_is_retained_while_recovery_warms_up(self):
        self.core('''
local g=Core.new();warm(g,0)
local s=snapshot(10);s.tick_ms=g.tick+351;g:ingest(s,1)
assert(not g:ready(1),'diagnostics cannot bypass freshness gate')
assert(g.reset_evidence and g.reset_evidence.cause=='NATIVE_GAP')
assert(g.reset_evidence.native_gap_ms==351 and math.abs(g.reset_evidence.poll_gap_s-0.1)<1e-8)
local evidence=g.reset_evidence
s.seq=11;s.tick_ms=s.tick_ms+100;g:ingest(s,1.1)
assert(g.reset_evidence==evidence,'normal warmup polling must retain the trigger')
''')

    def test_invalid_poll_gap_generation_and_jump_remain_distinct(self):
        cases = [
            ("g:ingest(nil,1)", 'INVALID_SNAPSHOT'),
            ("g:ingest(snapshot(10),1.3)", 'POLL_GAP'),
            ("g:ingest(snapshot(10),0.8)", 'CLOCK_ROLLBACK'),
            ("local s=snapshot(10);s.player.generation=2;g:ingest(s,1)", 'GENERATION_CHANGED'),
            ("local s=snapshot(10);s.player.xyz[1]=20;g:ingest(s,1)", 'PLAYER_JUMP'),
        ]
        for action, cause in cases:
            with self.subTest(cause=cause):
                self.core("local g=Core.new();warm(g,0);" + action + ";assert(g.reset_evidence and g.reset_evidence.cause=='" + cause + "')")

    def test_bad_or_absent_optional_monitor_version_does_not_fault_camera(self):
        for value in ['nil', '{}', '42']:
            with self.subTest(value=value):
                self.adapter("start();step(function(s) s.monitor_version="+value+" end);assert(ui():find('Native monitor: legacy / unavailable',1,true));step();assert(near(position[3],4))")

    def test_transition_records_include_gate_cause_and_native_publication(self):
        self.adapter('''
start();step(function(s) s.tick_ms=s.tick_ms+400;s.monitor_version='0.3.1';s.publication={failures=2,last_error=32} end)
tick=tick+0.21;callbacks.frame()
local found=false
for _,r in ipairs(records) do
 if r.kind=='transition' and r.channel=='camera' and r.from=='ACTIVE' and r.to=='WARMUP' then
  assert(r.context.native_gate and r.context.native_gate.cause=='NATIVE_GAP')
  assert(r.context.publication and r.context.publication.failures==2 and r.context.publication.last_error==32)
  found=true
 end
end
assert(found,'camera fallback must retain its trigger')
''')

    def test_collision_context_does_not_displace_publication_or_gate_evidence(self):
        self.exercise('''
start();layers[0]={hit(1,3,3.5,wall)};step()
step(function(s) s.tick_ms=s.tick_ms+400;s.publication={failures=2,last_error=32} end)
tick=tick+0.21;callbacks.frame()
local found=false
for _,r in ipairs(records) do
 if r.kind=='transition' and r.channel=='camera' and r.from=='ACTIVE' and r.to=='WARMUP' then
  assert(r.context.native_gate.cause=='NATIVE_GAP')
  assert(r.context.publication.failures==2)
  assert(r.context.collision_last.query.distance and r.context.collision_last.stabilization.distance)
  found=true
 end
end
assert(found)
''')
