"""Source-driven NAH HUD behavior through the real runtime adapter."""
import pathlib
import platform
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class HudTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.tmp.cleanup)
        cls.exe = pathlib.Path(cls.tmp.name)/'hud'
        names = ['native_runtime','native_fullbright','native_controller', 'native_fov', 'native_screen', 'native_av', 'native_head', 'native_bridge',
                 'original_scalar', 'original_state', 'original_inputs', 'original_collision',
                 'original_interaction', 'original_model']
        if (ROOT/'src/native_hud.c').exists():
            names.append('native_hud')
        extra = [str(ROOT/'src/native_bridge.S')]
        flags = []
        if platform.system() == 'Darwin':
            flags = ['-arch', 'x86_64']
            stub = pathlib.Path(cls.tmp.name)/'unexecuted_detours.c'
            stub.write_text('\n'.join(f'void native_detour_{i}(void){{}}' for i in range(78)))
            extra = [str(stub)]
        result = subprocess.run(['cc', *flags, '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                                 '-ffp-contract=off', '-fno-fast-math', '-I'+str(ROOT/'src'),
                                 str(ROOT/'tests/native_hud_host.c'),
                                 *[str(ROOT/'src'/f'{name}.c') for name in names], *extra,
                                 '-lm', '-o', str(cls.exe)], capture_output=True, text=True)
        if result.returncode:
            raise AssertionError(result.stderr)

    def check_case(self, name):
        result = subprocess.run([str(self.exe), name], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_oxygen_producer_emulates_full_movss(self): self.check_case('producer')
    def test_hidden_hud_emulates_original_stack_load_and_preserves_other_context(self): self.check_case('hidden')
    def test_oxygen_gate_is_exact_100_bits_and_switch_keeps_low_oxygen_hud(self): self.check_case('oxygen')
    def test_only_ready_nah_tp_hides_except_stopped_blood_fp(self): self.check_case('guards')
    def test_empty_hand_initial_load_clears_ammo(self): self.check_case('empty')
    def test_ammo_transition_hides_exactly_first_frame(self): self.check_case('ammo')
    def test_contamination_and_notices_keep_source_effects(self): self.check_case('notices')
    def test_opening_shaft_unsigned_bit_boundaries_and_timer(self): self.check_case('shaft')
    def test_bomb_two_conversations_and_no_further_checks(self): self.check_case('bomb')
    def test_main_room_entry_order_and_talk_end_reset(self): self.check_case('mainroom')
    def test_shield_first_dip_and_pic_conversations(self): self.check_case('freeze')
    def test_gauntlet_skips_three_conversations(self): self.check_case('gauntlet')
    def test_loading_seeds_shield_and_gauntlet_latches_without_writes(self): self.check_case('loading')
    def test_missing_replaced_and_revalidated_pointer_paths(self): self.check_case('identity')
    def test_partial_writes_rollback_and_never_mutate_replay_frame(self): self.check_case('failures')
    def test_f6_area_and_readiness_drop_hud_authority(self): self.check_case('reset')
    def test_aliases_cannot_mutate_stack_or_chain_before_replay(self): self.check_case('alias')
    def test_talk_preserves_forced_transmission_and_firstperson_gate(self): self.check_case('transmission')
    def test_owner_replacement_invalidates_prior_story_and_hide_latches(self): self.check_case('owners')
    def test_rollback_skips_owner_replaced_after_forward_store(self): self.check_case('rollback_replaced')
    def test_rollback_skips_owner_replaced_during_failed_write(self): self.check_case('rollback_failed_write')
    def test_rollback_revalidates_before_each_restoration(self): self.check_case('rollback_replaced_during_restore')
