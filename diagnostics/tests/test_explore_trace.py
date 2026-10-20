from pathlib import Path
import unittest
from lua_runner import run_lua
import test_explore_lua as camera_tests

ROOT = Path(__file__).resolve().parents[1]


class TraceTests(unittest.TestCase):
    def trace(self, scenario):
        p = ROOT/'lua/explore/trace.lua'
        self.assertTrue(p.exists(), 'transition trace not implemented')
        run_lua('local Trace=(function()\n'+p.read_text()+'\nend)()\n'+scenario)

    def test_transient_changes_survive_sampling_and_copy_scalar_context(self):
        self.trace('''
local t=Trace.new('session','run');local context={distance=1,player={1,2,3},bad=function() end}
t:observe('camera','ACTIVE',1,context)
t:observe('camera','FIRST_PERSON_AIM',1.01,context)
t:observe('camera','ACTIVE',1.02,context)
t:observe('camera','ACTIVE',1.03,context)
context.player[1]=999
local rows={};assert(t:drain(function(r) rows[#rows+1]=r;return true end))
assert(#rows==3 and rows[2].from=='ACTIVE' and rows[2].to=='FIRST_PERSON_AIM')
assert(rows[3].event_seq==3 and rows[3].run=='run')
assert(rows[1].context.player[1]==1 and rows[1].context.bad==nil)
''')

    def test_bounded_overflow_and_failed_sink_keep_accounting(self):
        self.trace('''
local t=Trace.new('s','r',4)
for i=1,9 do t:observe('camera','S'..i,i,{}) end
assert(t:stats().queued==4 and t:stats().dropped_total==5)
assert(not t:drain(function() error('I/O failed') end))
assert(t:stats().queued==4)
local rows={};assert(t:drain(function(r) rows[#rows+1]=r;return true end))
assert(#rows==4 and rows[1].event_seq==6 and rows[4].event_seq==9)
assert(rows[1].dropped_total==5 and t:stats().queued==0)
''')


class TraceAdapterTests(unittest.TestCase):
    adapter = camera_tests.ExploreAdapterTests.adapter

    def test_transition_flush_records_exact_reason_without_engine_io(self):
        self.adapter('''
start();phase='frame';callbacks.frame();local count=#records
for _,r in ipairs(records) do
 if r.kind=='transition' and r.to=='ACTIVE' then assert(r.context.native_reason=='READY') end
end
-- Change state twice inside one periodic sampling interval.
fixture=snapshot(seq+1);fixture.aim.raw=1;tick=tick+0.06;seq=seq+1
phase='frame';callbacks.frame();phase='lock';callbacks.LockScene()
assert(#records>=count);local before=#records
phase='update';callbacks.UpdateBehavior();phase='lock';callbacks.LockScene()
assert(#records==before,'no log I/O inside engine callbacks')
phase='frame';callbacks.frame()
local found=false
for _,r in ipairs(records) do
 if r.kind=='transition' and r.to=='FIRST_PERSON_AIM' then
  found=true;assert(r.context.aim==1 and r.context.native_seq==seq)
  assert(r.context.native_age_s>=0 and r.context.last_camera_player_distance)
 end
end
assert(found,'fallback event must flush before next periodic sample')
''')

    def test_logging_failure_does_not_disable_or_strand_camera(self):
        self.adapter('''
start();fail_io=true;step(function(s) s.aim.raw=1 end)
assert(near(position[3],3))
for i=1,12 do step() end;assert(near(position[3],4))
phase='reset';callbacks.reset();assert(near(position[3],3))
''')

    def test_camera_fault_is_retained_as_last_fallback(self):
        self.adapter("""start();fail_set=true;step()
assert(ui():find('Last fallback: FAULT_RESTORE_WRITE',1,true))
fail_set=false;step();assert(near(position[3],3))
""")

    def test_reset_flushes_off_event_and_last_fallback_is_kept_in_ui(self):
        self.adapter('''
start();step(function(s) s.inventory.raw=1 end)
for i=1,12 do step() end
assert(ui():find('Last fallback: FIRST_PERSON_INVENTORY',1,true))
phase='reset';callbacks.reset()
local found=false
for _,r in ipairs(records) do if r.kind=='transition' and r.to=='OFF' then found=true end end
assert(found,'reset emits and flushes OFF before closing journal')
''')
