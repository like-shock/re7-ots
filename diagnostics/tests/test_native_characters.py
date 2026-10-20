"""Run real native character producers/policy with only process reads substituted.

These characterize implemented behavior; production code is not changed to
manufacture a red phase. Each host scenario names the regression it catches.
"""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class NativeCharacterTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.tmp.cleanup)
        cls.binary = Path(cls.tmp.name) / "native_characters"
        sources = ("original_inputs", "original_state", "original_scalar", "original_collision")
        result = subprocess.run(
            ["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
             "-ffp-contract=off", "-fno-fast-math", "-I", str(ROOT / "src"),
             str(ROOT / "tests/native_characters_host.c"),
             *(str(ROOT / "src" / (name + ".c")) for name in sources),
             "-lm", "-o", str(cls.binary)],
            capture_output=True, text=True,
        )
        if result.returncode:
            raise RuntimeError(result.stdout + result.stderr)

    def run_case(self, number):
        result = subprocess.run([str(self.binary), str(number)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("native character contract passed", result.stdout)


for _number, _name in enumerate((
    "all_player_ids_and_dword_fallback",
    "shared_id_three_signed_boundary",
    "invalid_shared_id_retains_character_until_recovery",
    "joe_fallback_without_character_event",
    "character_specific_marker_offsets",
    "unreadable_or_nonfinite_marker_remains_optional",
    "all_character_gates_are_opt_in",
    "all_location_gates_are_opt_in",
    "scene_guards_and_melee_preference_remain_independent",
)):
    setattr(NativeCharacterTests, "test_" + _name, lambda self, n=_number: self.run_case(n))


if __name__ == "__main__":
    unittest.main()
