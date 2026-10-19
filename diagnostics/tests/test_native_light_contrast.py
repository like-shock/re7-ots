"""Original light-contrast read through the actual native runtime."""
import pathlib
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class LightContrastTests(unittest.TestCase):
    def test_original_unsigned_cap_and_register_read_only_contract(self):
        with tempfile.TemporaryDirectory() as directory:
            exe = pathlib.Path(directory) / 'contrast'
            names = ['native_runtime','native_av','native_head', 'native_bridge', 'original_scalar',
                     'original_state', 'original_inputs', 'original_collision',
                     'original_interaction', 'original_model']
            result = subprocess.run([
                'cc', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                '-ffp-contract=off', '-fno-fast-math', '-I'+str(ROOT/'src'),
                str(ROOT/'tests/native_light_contrast_host.c'),
                *[str(ROOT/'src'/f'{name}.c') for name in names],
                str(ROOT/'src/native_bridge.S'), '-lm', '-o', str(exe),
            ], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            result = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn('native light contrast contract passed', result.stdout)
