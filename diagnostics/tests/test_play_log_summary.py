import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

TOOL = Path(__file__).resolve().parents[1]/'tools/summarize_play_log.py'


def event(seq, state, previous=None, *, session='s', run='r', channel='camera', **extra):
    return {'kind':'transition','schema':1,'session':session,'run':run,'event_seq':seq,
            'channel':channel,'from':previous,'to':state,'lua_clock_s':seq/10,
            'dropped_total':0, **extra}


class PlayLogSummaryTests(unittest.TestCase):
    def setUp(self):
        self.assertTrue(TOOL.exists(), 'play log summary tool not implemented')
        spec=importlib.util.spec_from_file_location('play_summary',TOOL)
        self.mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(self.mod)
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name)

    def log(self, name, rows):
        p=self.root/name
        p.write_text(''.join(json.dumps(r)+'\n' for r in rows))
        return p

    def test_exact_transitions_and_rotated_duplicates(self):
        rows=[event(1,'ACTIVE'),event(2,'PAUSED_MENU','ACTIVE'),event(3,'ACTIVE','PAUSED_MENU'),
              event(4,'FIRST_PERSON_AIM','ACTIVE'),event(5,'ACTIVE','FIRST_PERSON_AIM')]
        a=self.log('play_0.jsonl',rows[:3]);b=self.log('play_1.jsonl',rows[2:])
        report=self.mod.summarize([b,a]);run=report['runs'][0]
        self.assertEqual(run['evidence'],'recorded_transitions')
        self.assertEqual(run['transition_count'],5)
        self.assertEqual(run['fallbacks'],{'PAUSED_MENU':1,'FIRST_PERSON_AIM':1})
        self.assertEqual(report['duplicates'],1)
        self.assertEqual(run['missing_event_sequences'],0)

    def test_sessions_and_lua_reloads_are_not_combined(self):
        p=self.log('mixed.jsonl',[event(1,'ACTIVE'),event(2,'WAIT_CAMERA_DISTANCE','ACTIVE'),
            event(1,'ACTIVE',run='r2'),event(2,'FIRST_PERSON_INVENTORY','ACTIVE',run='r2'),
            event(1,'ACTIVE',session='s2')])
        report=self.mod.summarize([p]);self.assertEqual(len(report['runs']),3)
        self.assertEqual(sum(r['transition_count'] for r in report['runs']),5)

    def test_legacy_samples_only_have_sampled_changes(self):
        p=self.log('old.jsonl',[{'kind':'header','schema':1,'session':'old','version':'0.3.0'}]+[
            {'kind':'sample','session':'old','lua_clock_s':i/5,'state':state}
            for i,state in enumerate(['ACTIVE','ACTIVE','FIRST_PERSON_AIM','ACTIVE'])])
        run=self.mod.summarize([p])['runs'][0]
        self.assertEqual(run['evidence'],'sampled_states_only')
        self.assertEqual(run['fallbacks'],{})
        self.assertEqual(run['sampled_changes'],{'FIRST_PERSON_AIM':1,'ACTIVE':1})

    def test_drop_gap_and_model_failure_evidence(self):
        p=self.log('gaps.jsonl',[event(6,'ACTIVE',dropped_total=5),
            event(9,'WRITE_FAILED','VISIBLE',channel='model',dropped_total=5)])
        run=self.mod.summarize([p])['runs'][0]
        self.assertEqual(run['dropped_total'],5)
        self.assertEqual(run['missing_event_sequences'],2)
        self.assertTrue(run['starts_mid_run'])
        self.assertEqual(run['model_failures'],{'WRITE_FAILED':1})

    def test_invalid_and_truncated_lines_are_counted_without_inference(self):
        p=self.log('broken.jsonl',[event(1,'ACTIVE'),event(True,'ACTIVE'),event(2,'ACTIVE',lua_clock_s=float('nan'))])
        with p.open('a') as f:f.write('{"partial":\n'+('['*2000)+']\n')
        report=self.mod.summarize([p]);self.assertEqual(report['invalid_lines'],4)
        self.assertEqual(report['runs'][0]['transition_count'],1)
        self.assertEqual(report['runs'][0]['fallbacks'],{})

    def test_conflicting_event_identity_is_excluded(self):
        p=self.log('conflict.jsonl',[event(1,'ACTIVE'),event(2,'FIRST_PERSON_AIM','ACTIVE'),
            event(2,'WAIT_CAMERA_DISTANCE','ACTIVE')])
        report=self.mod.summarize([p]);self.assertEqual(report['conflicts'],1)
        self.assertEqual(report['runs'][0]['fallbacks'],{})

    def test_cli_and_limits(self):
        p=self.log('play.jsonl',[event(1,'ACTIVE'),event(2,'FIRST_PERSON_AIM','ACTIVE')])
        result=subprocess.run([sys.executable,str(TOOL),'--json',str(p)],capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertEqual(json.loads(result.stdout)['runs'][0]['fallbacks'],{'FIRST_PERSON_AIM':1})
        result=subprocess.run([sys.executable,str(TOOL),str(p)],capture_output=True,text=True)
        self.assertIn('FIRST_PERSON_AIM',result.stdout)
        bad=subprocess.run([sys.executable,str(TOOL),str(self.root/'missing')],capture_output=True,text=True)
        self.assertEqual(bad.returncode,2);self.assertNotIn('Traceback',bad.stderr)
        self.mod.MAX_FILE_BYTES=10
        with self.assertRaises(ValueError):self.mod.summarize([p])
        self.mod.MAX_FILE_BYTES=100000
        p.write_text('x'*65537+'\n')
        with self.assertRaises(ValueError):self.mod.summarize([p])

    def test_generated_autorun_events_feed_the_summary_tool(self):
        from lua_runner import run_lua
        from test_explore_lua import FIXTURE
        root=TOOL.parents[1]
        subprocess.run([sys.executable,str(root/'tools/build_explore_lua.py')],check=True)
        destination=self.root/'actual.jsonl'
        before='local encode=(function()\n'+(root/'tests/json_encoder.lua').read_text()+'\nend)()\n'
        before+='local out=assert(require("io").open('+json.dumps(str(destination))+',"wb"))\n'
        before+='json.dump_string=function(row) local line=encode(row);assert(out:write(line.."\\n"));return line end\n'
        bundle=(root/'build/re7_ots_explore.lua').read_text()
        harness=(root/'tests/explore_lua_harness.lua').read_text().replace('-- INSERT ADAPTER HERE',before+bundle)
        run_lua(FIXTURE+harness+"""
start();step(function(s) s.aim.raw=1 end)
for i=1,12 do step() end
fail_json=true;step();fail_json=false
for i=1,12 do step() end
key=true;step();key=false;phase='reset';callbacks.reset();out:close()
""")
        report=self.mod.summarize([destination]);run=report['runs'][0]
        self.assertEqual(report['invalid_lines'],0)
        self.assertEqual(run['fallbacks'],{'FIRST_PERSON_AIM':1,'WAIT_NATIVE_INVALID':1})
        self.assertEqual(run['dropped_total'],0)
        self.assertEqual(run['missing_event_sequences'],0)
