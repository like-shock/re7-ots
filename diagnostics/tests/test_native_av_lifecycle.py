"""Real producer ordering, readiness loss, AV authority and F6 independence."""
from pathlib import Path
import subprocess
import tempfile
import unittest
from test_native_fov import compile_host

ROOT = Path(__file__).resolve().parents[1]


class AudiovisualLifecycleTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.tmp.cleanup)
        cls.exe = compile_host(Path(cls.tmp.name), ROOT/'tests/native_av_lifecycle_host.c')

    def check_case(self, case):
        result = subprocess.run([str(self.exe), case], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_f6_off_flashlight_collection_cannot_authorize_av(self):
        self.check_case('f6')

    def test_timescale_read_failure_uses_same_invalidation(self):
        self.check_case('timescale')


for _loss in ('loading', 'failed_required_input'):
    for _path, _name in enumerate(('alternate_light', 'audio', 'beam', 'color', 'shadow')):
        setattr(AudiovisualLifecycleTests, 'test_'+_loss+'_'+_name,
                lambda self, case=('l' if _loss == 'loading' else 'f')+str(_path): self.check_case(case))
