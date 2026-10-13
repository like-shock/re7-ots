import pathlib, subprocess, unittest
ROOT=pathlib.Path(__file__).resolve().parents[1]

class ExplorePluginTests(unittest.TestCase):
 def test_automatic_monitor_and_failures(self):
  self.assertTrue((ROOT/'src/explore_plugin.c').exists(), 'automatic monitor not implemented')
  subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-DDIAG_HOST_TEST','-Isrc','tests/explore_host.c','src/observer.c','src/core.c','-o','build/explore_host'],cwd=ROOT,check=True)
  for name in ['normal','hash','read','write','short','rename','thread','focus','pe','exe','rename_once','initial_rename_once']:
   with self.subTest(name=name):
    p=subprocess.run([str(ROOT/'build/explore_host'),name],capture_output=True,text=True,timeout=5)
    self.assertEqual(p.returncode,0,p.stdout+p.stderr)
