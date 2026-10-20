"""Trace real Fullbright transitions without changing their game effects."""
from pathlib import Path
import json
import subprocess
import tempfile
import unittest
from test_native_fov import compile_host

ROOT = Path(__file__).resolve().parents[1]


class FullbrightTraceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.tmp.cleanup)
        cls.exe = compile_host(Path(cls.tmp.name), ROOT/"tests/native_fullbright_trace_host.c")

    def check(self, case):
        result = subprocess.run([str(self.exe), case], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_readiness_loss_view_transition_and_light_reacquisition(self):
        self.check("transitions")

    def test_distinguishes_light_read_identity_stack_and_global_failures(self):
        self.check("failures")

    def test_independent_first_person_bootstrap_and_loading_reason(self):
        self.check("fp")

    def test_mode_and_option_transitions_record_even_without_a_next_hook(self):
        self.check("mode")

    def test_event_buffer_is_bounded_without_affecting_lighting(self):
        self.check("bounded")


class FullbrightLogTests(unittest.TestCase):
    def test_worker_formats_valid_bounded_json_for_events_and_counters(self):
        plugin = (ROOT/"src/native_plugin.c").read_text()
        start = plugin.index("static void float_text(")
        end = plugin.index("static void snapshot(", start)
        host = r'''#define main original_trace_cases
#include "native_optional_host.c"
#undef main
#include <stdarg.h>
#include <math.h>
static uint64_t generation=7;
static int log_failed;
static void line(const char*fmt,...){
 char buffer[8192];va_list args;va_start(args,fmt);
 int n=vsnprintf(buffer,sizeof buffer,fmt,args);va_end(args);
 CHECK(n>0&&(size_t)n<sizeof buffer);fputs(buffer,stdout);
}
'''+plugin[start:end]+r'''
int main(void){
 setup();CHECK(native_runtime_set_fullbright(&r,1,0xffffff,1));
 CHECK(call(NS_LIGHT_SOURCE_DIAMETER_READ));CHECK(call(NS_GLOBAL_BRIGHTNESS));
 r.inputs.state.igtimer=NAN;r.inputs.player_base=UINT64_MAX;
 NativeSnapshot s;native_runtime_snapshot(&r,&s);
 s.fullbright_failures[NFB_FAILURE_STACK_WRITE]=UINT64_MAX;
 s.fullbright_events_dropped=UINT64_MAX;
 fullbright_log(1234,&s);
 NativeFullbrightEvent e=s.fullbright_state;e.sequence=99;e.site=NS_G_BRIGHTNESS_2;
 fullbright_event_log(1234,&e);
 CHECK(!log_failed);return 0;
}
'''
        with tempfile.TemporaryDirectory() as directory:
            tmp=Path(directory)
            (tmp/"log.c").write_text(host)
            exe=compile_host(tmp,tmp/"log.c")
            result=subprocess.run([str(exe)],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)
        rows=[json.loads(row) for row in result.stdout.splitlines()]
        self.assertEqual(len(rows),2)
        snapshot,event=rows
        self.assertEqual(snapshot["type"],"fullbright_snapshot")
        self.assertEqual(snapshot["generation"],7)
        self.assertEqual(snapshot["consumers"]["diameter"],{"calls":1,"applied":1})
        self.assertEqual(snapshot["consumers"]["global"],{"calls":1,"applied":1})
        self.assertEqual(snapshot["failures"]["stack_write_failed"],2**64-1)
        self.assertEqual(snapshot["events_dropped"],2**64-1)
        self.assertIsNone(snapshot["state"]["game_time"])
        self.assertEqual(snapshot["state"]["player"],"0xffffffffffffffff")
        self.assertEqual(event["type"],"fullbright_event")
        self.assertEqual(event["state"]["sequence"],99)
        self.assertEqual(event["state"]["gate"],"ready")
        self.assertIsNone(event["state"]["game_time"])
