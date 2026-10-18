"""Head target ownership, exact original transforms, and fail-closed reads."""
import pathlib
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class NativeHeadTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.directory.cleanup)
        cls.exe = pathlib.Path(cls.directory.name) / 'native_head'
        result = subprocess.run([
            'cc', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
            '-ffp-contract=off', '-fno-fast-math', '-I'+str(ROOT/'src'),
            str(ROOT/'tests/native_head_host.c'), str(ROOT/'src/native_head.c'),
            '-o', str(cls.exe),
        ], capture_output=True, text=True)
        if result.returncode:
            raise AssertionError(result.stderr)

    def run_case(self, case):
        result = subprocess.run([str(self.exe), case], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('native head contract passed', result.stdout)


for _case in ('cache_clear', 'every_playable_character', 'exact_head_transforms',
              'character_change', 'exact_marker', 'unreadable_head_chain',
              'changing_head_chain', 'reloaded_head', 'invalid_refresh',
              'invalid_spans', 'shadow_targets', 'present_mia_hair',
              'optional_cache_reset', 'unreadable_optional_chains',
              'reloaded_optional_targets', 'changing_optional_chains',
              'invalid_optional_spans'):
    setattr(NativeHeadTests, 'test_' + _case,
            lambda self, case=_case: self.run_case(case))


if __name__ == '__main__':
    unittest.main()
