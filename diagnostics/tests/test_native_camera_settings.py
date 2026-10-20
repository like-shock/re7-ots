"""Exercise settings migration and their actual camera/FOV consumers."""
from pathlib import Path
import subprocess
import tempfile
import unittest
from test_native_fov import compile_host

ROOT = Path(__file__).resolve().parents[1]


class CameraSettingsTests(unittest.TestCase):
    def test_old_settings_migration_range_validation_roundtrip_and_save_race(self):
        with tempfile.TemporaryDirectory() as directory:
            exe = Path(directory) / "settings"
            result = subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                                     "-I"+str(ROOT/"src"), str(ROOT/"tests/native_camera_settings_host.c"),
                                     str(ROOT/"src/native_options.c"), "-lm", "-o", str(exe)],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            subprocess.run([str(exe)], check=True)

    def test_live_camera_fov_shoulder_zero_and_reframework_handoff(self):
        with tempfile.TemporaryDirectory() as directory:
            exe = compile_host(Path(directory), ROOT/"tests/native_camera_runtime_host.c")
            result = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
