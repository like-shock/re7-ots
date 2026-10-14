import unittest
import test_explore_lua as camera


class SnapshotReadTests(unittest.TestCase):
    adapter = camera.ExploreAdapterTests.adapter

    def test_single_empty_or_exception_read_recovers_without_camera_drop(self):
        for failure in ["return nil", "error('open/parse failed')"]:
            with self.subTest(failure=failure):
                self.adapter("""
start();local original=json.load_file;local reads=0
json.load_file=function(p) reads=reads+1;if reads==1 then """+failure+""" end;return original(p) end
step();assert(reads==2,'one immediate retry');assert(near(position[3],4),'no FP drop')
tick=tick+0.201;callbacks.frame()
local s=records[#records];assert(s.kind=='sample');assert(s.native_read.recovered_total==1)
""")

    def test_persistent_failure_is_bounded_and_restores_immediately(self):
        self.adapter("""
start();local reads=0;json.load_file=function() reads=reads+1;return nil end
step();assert(reads==2);assert(near(position[3],3));assert(ui():find('WAIT_NATIVE_INVALID',1,true))
step();assert(reads==4,'two attempts per poll, no sleep or retry loop')
""")

    def test_decoded_invalid_values_are_not_retried(self):
        for invalid in ['false','42','{}',"{schema=2}"]:
            with self.subTest(invalid=invalid):
                self.adapter("""
start();local reads=0;json.load_file=function() reads=reads+1;return """+invalid+""" end
step();assert(reads==1,'decoded invalid data must not be hidden');assert(near(position[3],3))
""")

    def test_retry_result_still_obeys_gameplay_identity_and_freshness_gates(self):
        for mutation in ['s.aim.raw=1','s.player.status="NULL_LINK"','s.player.generation=2',
                         's.seq=1;s.tick_ms=1100','s.version="other"']:
            with self.subTest(mutation=mutation):
                self.adapter("""
start();local original=json.load_file;local reads=0
json.load_file=function(p) reads=reads+1;if reads==1 then return nil end;return original(p) end
step(function(s) """+mutation+""" end)
assert(reads==2);assert(near(position[3],3),'retry must not bypass gate')
""")
