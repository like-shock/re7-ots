import pathlib, subprocess, unittest
ROOT=pathlib.Path(__file__).resolve().parents[1]
class ObserverPluginTests(unittest.TestCase):
 def test_synthetic_worker_session_and_failures(self):
  self.assertTrue((ROOT/'src/observer_plugin.c').exists(), 'native observer wrapper is not implemented')
  subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-DDIAG_HOST_TEST','-Isrc','tests/observer_host.c','src/observer.c','src/core.c','-o','build/observer_host'],cwd=ROOT,check=True)
  for scenario in ['normal','hash_mismatch','read_fail','write_fail','partial_write','time_limit','sample_limit','size_limit','thread_fail','focus','stopped_validation','session_limit']:
   p=subprocess.run([str(ROOT/'build/observer_host'),scenario],capture_output=True,text=True,timeout=10)
   self.assertEqual(p.returncode,0,scenario+'\n'+p.stdout+p.stderr)
