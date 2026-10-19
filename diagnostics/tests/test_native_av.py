"""RT 8.02 default audiovisual hook behavior, including replay boundaries."""
import pathlib
import platform
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class AudiovisualTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.tmp.cleanup)
        cls.exe = pathlib.Path(cls.tmp.name)/'av'
        names = ['native_runtime','native_fullbright','native_controller','native_fov','native_hud','native_screen', 'native_head', 'native_bridge', 'original_scalar',
                 'original_state', 'original_inputs', 'original_collision',
                 'original_interaction', 'original_model']
        if (ROOT/'src/native_av.c').exists():
            names.append('native_av')
        extra = [str(ROOT/'src/native_bridge.S')]
        flags = []
        if platform.system() == 'Darwin':
            # Adapter tests only; the Linux CI suite executes the real bridge.
            flags = ['-arch', 'x86_64']
            stub = pathlib.Path(cls.tmp.name)/'unexecuted_detours.c'
            stub.write_text('\n'.join(f'void native_detour_{i}(void){{}}' for i in range(78)))
            extra = [str(stub)]
        result = subprocess.run(['cc', *flags, '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                        '-ffp-contract=off', '-fno-fast-math', '-I'+str(ROOT/'src'),
                        str(ROOT/'tests/native_av_host.c'),
                        *[str(ROOT/'src'/f'{name}.c') for name in names], *extra,
                        '-lm', '-o', str(cls.exe)], capture_output=True, text=True)
        if result.returncode:
            raise AssertionError(result.stderr)

    def check_case(self, name):
        result = subprocess.run([str(self.exe), name], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_steady_light_broadcasts_all_lanes(self): self.check_case('steady')
    def test_flashlight_position_and_transition_exceptions(self): self.check_case('position')
    def test_flashlight_color_quantization_and_register_effects(self): self.check_case('color')
    def test_beam_width_requires_observed_exact_flashlight(self): self.check_case('beam')
    def test_audio_uses_raw_camera_preserving_source_registers(self): self.check_case('audio')
    def test_head_bob_takes_original_je_with_cmp_flags(self): self.check_case('wobble')
    def test_cutscene_blur_preserves_firstperson_and_menu(self): self.check_case('blur')
    def test_vhs_predicates_and_spider_exception(self): self.check_case('vhs')
