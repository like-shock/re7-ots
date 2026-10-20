import importlib.util,pathlib,tempfile,json,unittest
ROOT=pathlib.Path(__file__).resolve().parents[1]
class ProbeReportTests(unittest.TestCase):
 def test_incomplete_capture_is_never_reported_as_compatible(self):
  spec=importlib.util.spec_from_file_location('report',ROOT/'tools/summarize_probe.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
  with tempfile.TemporaryDirectory() as d:
   p=pathlib.Path(d)/'probe.jsonl'
   def write(rows):p.write_text(''.join(json.dumps(x)+'\n' for x in rows))
   header={'type':'header','schema':1,'mode':'PASS_THROUGH_HOOK_PROBE'}
   write([header,{'type':'summary','status':'READY','reason':'READY_F6'}])
   self.assertEqual(m.summarize(p)['result'],'NOT_CAPTURED')
   summary={'type':'summary','status':'CAPTURE_FINISHED_REVIEW_REQUIRED','reason':'TIME_LIMIT','camera_target_matches':10}
   rows=[header,summary]+[{'type':'site_summary','site':i,'calls':10,'dropped':0,'logged':2,'sampled_out':8} for i in range(5)]
   write(rows);self.assertEqual(m.summarize(p)['result'],'DEVICE_EVIDENCE_REQUIRES_REVIEW')
   rows[-1]['calls']=0;write(rows);self.assertEqual(m.summarize(p)['missing_sites'],[4])
   p.write_text(p.read_text()+'{truncated');self.assertTrue(m.summarize(p)['truncated_or_invalid_lines'])
   write([header,{'type':'summary','status':'HOOK_INSTALL_FAILED','reason':'ENABLE_STATUS_10'}])
   self.assertEqual(m.summarize(p)['result'],'BLOCKED')
