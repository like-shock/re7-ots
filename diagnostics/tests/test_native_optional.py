"""Optional adapters: complete frames, source predicates and lifecycle edges."""
import pathlib, platform, subprocess, tempfile, unittest
ROOT=pathlib.Path(__file__).resolve().parents[1]
class OptionalTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.tmp=tempfile.TemporaryDirectory();cls.addClassCleanup(cls.tmp.cleanup)
  cls.exe=pathlib.Path(cls.tmp.name)/'optional'
  names=['native_runtime','native_fov','native_hud','native_screen','native_av','native_head','native_fullbright','native_controller','native_bridge','original_scalar','original_state','original_inputs','original_collision','original_interaction','original_model']
  extra=[str(ROOT/'src/native_bridge.S')];flags=[]
  if platform.system()=='Darwin':
   flags=['-arch','x86_64'];stub=pathlib.Path(cls.tmp.name)/'stub.c'
   stub.write_text('\n'.join(f'void native_detour_{i}(void){{}}' for i in range(78)));extra=[str(stub)]
  result=subprocess.run(['cc',*flags,'-std=c11','-O2','-Wall','-Wextra','-Werror','-ffp-contract=off','-fno-fast-math','-I'+str(ROOT/'src'),str(ROOT/'tests/native_optional_host.c'),*[str(ROOT/'src'/f'{x}.c') for x in names],*extra,'-lm','-o',str(cls.exe)],capture_output=True,text=True)
  if result.returncode:raise AssertionError(result.stderr)
 def check(self,n):
  p=subprocess.run([str(self.exe),n],capture_output=True,text=True);self.assertEqual(p.returncode,0,p.stderr)
 def test_shader_cmp_orders_hurt_priority_and_failure_frames(self):self.check('shader')
 def test_fullbright_main_vector_stack_ecx_and_color(self):self.check('light')
 def test_fullbright_identity_replacement_disabled_and_loading(self):self.check('identity')
 def test_fullbright_radius_cmp_branches_global_and_shadow(self):self.check('consumers')
 def test_fullbright_first_person_cold_start_toggle_and_isolation(self):self.check('fullbright_fp')
 def test_fullbright_first_person_loading_owner_change_and_recovery(self):self.check('fullbright_fp_loading')
 def test_fullbright_first_person_nah_timer_and_player_replacement(self):self.check('fullbright_fp_nah')
 def test_fullbright_reenable_requires_fresh_observers_after_mode_zero(self):self.check('fullbright_fp_reenable')
 def test_controller_all_24_source_buttons_have_clean_edges(self):self.check('buttons')
 def test_controller_load_reconnect_rebind_timeout_no_phantom(self):self.check('controller_reset')
 def test_controller_floats_producer_replacement_discards_both_queues(self):self.check('owner_floats')
 def test_controller_write_producer_replacement_discards_both_queues(self):self.check('owner_write')
 def test_optional_validation_and_off_mode_dispatch(self):self.check('off')

 def test_connection_mask_change_and_unavailable_observer_fail_closed(self):self.check('reconnect')
