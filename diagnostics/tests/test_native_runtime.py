"""Actual native adapter contract: complete stores and original register effects."""
import pathlib, subprocess, tempfile, unittest
ROOT=pathlib.Path(__file__).resolve().parents[1]
class RuntimeTests(unittest.TestCase):
 def test_runtime_contract(self):
  self.run_host("native_runtime_host")
 def test_head_runtime_contract(self):
  self.run_host("native_head_runtime_host")
 def run_host(self,name):
  with tempfile.TemporaryDirectory() as d:
   exe=pathlib.Path(d)/'runtime'
   names=['native_runtime','native_av','native_head','native_bridge','original_scalar','original_state','original_inputs','original_collision','original_interaction','original_model']
   p=subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-ffp-contract=off','-fno-fast-math','-I'+str(ROOT/'src'),str(ROOT/'tests'/f'{name}.c'),*[str(ROOT/'src'/f'{n}.c') for n in names],str(ROOT/'src/native_bridge.S'),'-lm','-o',str(exe)],capture_output=True,text=True)
   self.assertEqual(p.returncode,0,p.stderr)
   p=subprocess.run([str(exe)],capture_output=True,text=True)
   self.assertEqual(p.returncode,0,p.stderr)
   self.assertIn('passed',p.stdout)
