import hashlib,json,pathlib,struct,subprocess,tempfile,unittest
ROOT=pathlib.Path(__file__).resolve().parents[1]
MANIFEST=json.loads((ROOT/'data/patch_manifest.json').read_text());BYNAME={p['name']:p for p in MANIFEST['patches']}
SELECT=['CAM_WRITES','HEAD_Y_WRITE','FIRING_COOR_WRITES','FOV_WRITE_MAIN','FOV_WRITE_NAH']
def fixture(mode):
 raw=bytearray(b'\xCC'*0x18400);raw[:1024]=b'\0'*1024;raw[:2]=b'MZ';struct.pack_into('<I',raw,60,128);raw[128:132]=b'PE\0\0';struct.pack_into('<HHI',raw,132,0x8664,1,123456);struct.pack_into('<H',raw,148,240);struct.pack_into('<H',raw,152,0x20b);struct.pack_into('<Q',raw,176,0x140000000);struct.pack_into('<II',raw,208,0x30000,0x400)
 raw[392:397]=b'.text';struct.pack_into('<IIII',raw,400,0x18000,0x10000,0x18000,0x400);struct.pack_into('<I',raw,428,0x60000020)
 anchors={}
 for i,name in enumerate(SELECT):
  p=BYNAME[name];rva=0x12000+i*0x400;anchors[name]=rva;offset=rva-0x10000+0x400
  tokens=p['pattern'].split();pattern=bytes(int(t,16) if '?' not in t and '*' not in t else 0 for t in tokens);raw[offset:offset+len(pattern)]=pattern
  orig=bytes.fromhex(p['expected']);pos=offset+p['delta'];raw[pos:pos+len(orig)]=orig
 if mode=='duplicate':
  p=BYNAME['HEAD_Y_WRITE'];off=0x400+0x7000;raw[off:off+6]=bytes.fromhex(p['pattern'])
 if mode=='missing':raw[0x400+0x2400]=0xCC
 mem=bytearray(b'\0'*0x30000);mem[:0x400]=raw[:0x400];mem[0x10000:0x28000]=raw[0x400:0x18400]
 if mode=='changed':mem[anchors['CAM_WRITES']+BYNAME['CAM_WRITES']['delta']]=0xe9
 if mode=='bad_disk':raw[0]=0
 return bytes(raw),bytes(mem)
class HostTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Wno-misleading-indentation','-DDIAG_HOST_TEST','-Isrc','tests/host.c','src/plugin.c','src/core.c','-o','build/test_host'],cwd=ROOT,check=True)
 def run_host(self,mode):
  raw,mem=fixture(mode)
  with tempfile.TemporaryDirectory(prefix='re7_diag_test_') as d:
   path=pathlib.Path(d);(path/'re7.exe').write_bytes(raw);(path/'image.bin').write_bytes(mem)
   p=subprocess.run([str(ROOT/'build/test_host'),d,mode,str(path/'image.bin')],capture_output=True,text=True)
   self.assertEqual(p.returncode,0,p.stdout+p.stderr);self.assertEqual((path/'re7.exe').read_bytes(),raw)
   logs=[f.read_text() for f in sorted(path.glob('reframework/data/RE7_OTS_Diagnostics/*.txt'))]
   status=path/'reframework/data/RE7_OTS_Diagnostics/status.json'
   try:self.last_status=json.loads(status.read_text()) if status.exists() else None
   except (ValueError,OSError):self.last_status=None
   return logs,p.stdout+p.stderr,raw
 def patch(self,log,name):
  return next(s for s in log.split('[PATCH ') if '\nname='+name+'\n' in s)
 def test_status_file_indicates_complete(self):
  self.run_host('normal');self.assertIsNotNone(self.last_status);self.assertEqual(self.last_status['phase'],'COMPLETE');self.assertTrue(self.last_status['report'].endswith('.txt'))
 def test_normal_with_inaccessible_header_gap(self):
  logs,_,raw=self.run_host('normal');self.assertEqual(len(logs),1);s=logs[0]
  self.assertIn('status=COMPLETE',s);self.assertIn('exe_sha256='+hashlib.sha256(raw).hexdigest(),s)
  for name in SELECT:self.assertIn('assessment=BYTE_MATCH_ONLY\n',self.patch(s,name))
 def test_whole_patch_change_outside_aob_is_detected(self):
  logs,_,_=self.run_host('changed');s=self.patch(logs[0],'CAM_WRITES');self.assertIn('memory_match_count=1\n',s);self.assertIn('assessment=LIVE_BYTES_CHANGED_AT_DISK_CANDIDATE',s)
 def test_duplicate_is_not_a_pass(self):
  logs,_,_=self.run_host('duplicate');self.assertIn('assessment=AMBIGUOUS_PATTERN',self.patch(logs[0],'HEAD_Y_WRITE'))
 def test_missing_is_reported(self):
  logs,_,_=self.run_host('missing');self.assertIn('assessment=PATTERN_NOT_FOUND',self.patch(logs[0],'HEAD_Y_WRITE'))
 def test_unreadable_memory_is_not_complete(self):
  logs,_,_=self.run_host('unreadable');s=logs[0];self.assertIn('memory_readable_scope_complete=NO',s);self.assertIn('memory_failed_read_bytes=4096',s);self.assertIn('assessment=BYTE_MATCH_ONLY_INCOMPLETE_SCAN',self.patch(s,'CAM_WRITES'))
 def test_file_denied_still_reports_live(self):
  logs,_,_=self.run_host('file_denied');self.assertIn('status=COMPLETE',logs[0]);self.assertIn('assessment=LIVE_BYTE_MATCH_DISK_UNAVAILABLE',self.patch(logs[0],'CAM_WRITES'))
 def test_invalid_disk_still_reports_live(self):
  logs,_,_=self.run_host('bad_disk');self.assertIn('disk_pe=INVALID_OR_UNSUPPORTED',logs[0]);self.assertIn('assessment=LIVE_BYTE_MATCH_DISK_UNAVAILABLE',self.patch(logs[0],'CAM_WRITES'))
 def test_second_scan_does_not_reuse_disk_counts(self):
  logs,_,_=self.run_host('repeat');self.assertEqual(len(logs),2);self.assertIn('disk_scanned_bytes=0\n',logs[1]);self.assertIn('assessment=LIVE_BYTE_MATCH_DISK_UNAVAILABLE',self.patch(logs[1],'CAM_WRITES'))
 def test_log_creation_failure_is_explicit(self):
  logs,out,_=self.run_host('log_denied');self.assertFalse(logs);self.assertIn('Could not create diagnostic log',out)
 def test_log_write_failure_is_explicit(self):
  _,out,_=self.run_host('log_write_fail');self.assertIn('Diagnostic log write failed',out)
if __name__=='__main__':unittest.main()
