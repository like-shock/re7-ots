"""RT 8.02 basic screen and Critter producer/consumer behavior."""
import pathlib
import platform
import subprocess
import tempfile
import unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]


class ScreenTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.tmp.cleanup)
        cls.exe = pathlib.Path(cls.tmp.name)/'screen'
        names = ['native_runtime', 'native_av', 'native_head', 'native_bridge',
                 'original_scalar', 'original_state', 'original_inputs',
                 'original_collision', 'original_interaction', 'original_model']
        if (ROOT/'src/native_screen.c').exists():
            names.append('native_screen')
        extra = [str(ROOT/'src/native_bridge.S')]
        flags = []
        if platform.system() == 'Darwin':
            flags = ['-arch', 'x86_64']
            stub = pathlib.Path(cls.tmp.name)/'unexecuted_detours.c'
            stub.write_text('\n'.join(f'void native_detour_{i}(void){{}}' for i in range(64)))
            extra = [str(stub)]
        result = subprocess.run(['cc', *flags, '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                                 '-ffp-contract=off', '-fno-fast-math', '-I'+str(ROOT/'src'),
                                 str(ROOT/'tests/native_screen_host.c'),
                                 *[str(ROOT/'src'/f'{name}.c') for name in names], *extra,
                                 '-lm', '-o', str(cls.exe)], capture_output=True, text=True)
        if result.returncode:
            raise AssertionError(result.stderr)

    def check_case(self, name):
        result = subprocess.run([str(self.exe), name], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_item_scan_produces_critter_and_scrap_without_stale_matches(self): self.check_case('items')
    def test_eoz_policy_consumes_observed_critter(self): self.check_case('policy')
    def test_vignette_suppresses_thirdperson_only(self): self.check_case('vignette')
    def test_psycho_producer_replays_full_movss_and_sets_pulse(self): self.check_case('timer')
    def test_distortion_decrements_before_check_and_copies_full_vector(self): self.check_case('distortion')
    def test_nah_dot_uses_live_aim_and_preserves_original_lea(self): self.check_case('dot')
    def test_nah_redline_sets_cmp_equal_flags(self): self.check_case('redline')
    def test_f6_loading_and_missing_readiness_clear_transient_state(self): self.check_case('reset')
