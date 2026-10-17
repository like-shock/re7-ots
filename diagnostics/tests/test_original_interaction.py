"""Source-derived interaction-gate cases from RT 8.02 CT lines 1459-1948.

Memory is a byte-addressed fixture, not live pointers. These cases catch a
global object shift, rbx/r13 identity confusion, stale target reuse, loss of
the pickup latch, prompt TTL, and unsigned-bit comparisons turned into floats.
"""
import ctypes as C
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
BASE = 0x140000000
TARGET = 0x21000000B0
BAD = 0x69
INTERACTABLE = 0x08FB17F0
FPROMPT = 0x08FB2BA0
POPUP = 0x08FB16E0
INVENTORY = 0x08FAC380
READ = C.CFUNCTYPE(C.c_int, C.c_void_p, C.c_uint64, C.c_void_p, C.c_size_t)


class Input(C.Structure):
    _fields_ = [(n, C.c_uint8) for n in (
        'tpinteractables', 'firstpersonflag', 'fixpaperscan', 'fixwormbox',
        'fixfprompt', 'fixmain', 'interactableflag', 'scrapexamine', 'lockboxflag')]
    _fields_ += [('loadarea', C.c_uint32), ('frametrack', C.c_uint32),
                ('cplayerz', C.c_float), ('invflagadd', C.c_uint64),
                ('secintadd', C.c_uint64)]


class State(C.Structure):
    _fields_ = [(n, C.c_uint8) for n in (
        'paperscanpickuppop', 'readingnote', 'herbinwormbox', 'itempickuppop')]
    _fields_ += [('oldfpromptframe', C.c_uint32), ('activefprompt', C.c_uint32),
                ('pvmainadd', C.c_uint64), ('ivmainadd', C.c_uint64)]


class Memory:
    def __init__(self):
        self.data, self.reads = {}, []
        self.callback = READ(self.read)

    def put(self, address, value, width=8):
        for i, byte in enumerate(value.to_bytes(width, 'little')):
            self.data[address + i] = byte

    def read(self, context, address, output, count):
        self.reads.append((address, count))
        if any(address + i not in self.data for i in range(count)):
            return 0
        C.memmove(output, bytes(self.data[address + i] for i in range(count)), count)
        return 1

    def chain(self, root, offsets, end):
        current = 0x3000000100 + root * 16
        self.put(BASE + root, current)
        for i, offset in enumerate(offsets):
            following = end if i == len(offsets) - 1 else current + 0x1000
            self.put(current + offset, following)
            current = following

    def paper(self, target=TARGET):
        self.chain(INTERACTABLE, [0x98, 0x18], target - 0xB0)

    def main(self, target=TARGET):
        # Reuse the same interactable root as paper when both are present.
        root = 0x3000000100 + INTERACTABLE * 16
        self.put(BASE + INTERACTABLE, root)
        self.put(root + 0x348, root + 0x5000)
        self.put(root + 0x5010, root + 0x6000)
        self.put(root + 0x60C0, target - 0x74)

    def prompt(self, target=TARGET):
        self.chain(FPROMPT, [0x18, 0x60, 0x68, 0x18, 0xC0, 0x238, 0x18], target - 0xB0)

    def popup(self, status):
        end = 0x3600000000
        self.chain(POPUP, [0x110, 0x70], end)
        self.put(end + 0xA0E, status, 2)


class OriginalInteractionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.tmp.cleanup)
        so = Path(cls.tmp.name) / 'interaction.so'
        subprocess.run(['cc', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                        '-shared', '-fPIC', str(ROOT/'src/original_interaction.c'),
                        '-o', str(so)], check=True)
        cls.lib = C.CDLL(str(so))
        declarations = {
            'select': ([C.POINTER(Input), C.POINTER(State), C.c_uint64, C.c_uint64,
                        C.c_uint64, READ, C.c_void_p], C.c_int),
            'track_main': ([C.POINTER(Input), C.POINTER(State), C.c_uint64,
                            C.c_uint64, READ, C.c_void_p], C.c_int),
            'prompt_seen': ([C.POINTER(State), C.c_uint8], None),
            'loadscreen': ([C.POINTER(State), C.c_uint8], None),
            'timer': ([C.c_uint32], C.c_uint32),
            'secondary': ([C.c_uint64, READ, C.c_void_p, C.POINTER(C.c_uint64)], C.c_int),
            'inventory': ([C.c_uint64, READ, C.c_void_p, C.POINTER(C.c_uint64)], C.c_int),
            'scrap': ([C.c_uint64, READ, C.c_void_p], C.c_uint8),
            'lockbox': ([C.c_uint32, C.c_float, C.c_float, C.c_float], C.c_uint8),
        }
        for name, (args, result) in declarations.items():
            fn = getattr(cls.lib, 're7_original_interaction_' + name)
            fn.argtypes, fn.restype = args, result
            setattr(cls, name, staticmethod(fn))

    def setUp(self):
        self.memory = Memory()
        self.memory.paper()
        self.input = Input(1, 0, 1, 1, 1, 1, 1, 0, 0)
        self.state = State()

    def choose(self, target=TARGET, r13=0):
        return self.select(C.byref(self.input), C.byref(self.state), BASE,
                           target - 0x30, r13, self.memory.callback, None)

    def track(self, target=TARGET):
        return self.track_main(C.byref(self.input), C.byref(self.state), BASE,
                               target - 0x34, self.memory.callback, None)

    def prompt_only(self):
        self.input.fixpaperscan = self.input.fixwormbox = 0
        self.state.ivmainadd = TARGET + 0x1000
        self.state.activefprompt = 3
        self.memory.prompt()

    def test_paper_requires_exact_rbx_plus_30_and_never_every_object(self):
        self.assertEqual(self.choose(TARGET + 4), 0)
        self.assertEqual(self.state.pvmainadd, TARGET)
        self.assertEqual(self.choose(), 1)
        self.assertEqual(self.choose(TARGET - 4), 0)

    def test_tp_fp_and_static_low_dword_gates_precede_target_scans(self):
        self.input.tpinteractables = 0
        self.assertEqual(self.choose(), 0)
        self.input.tpinteractables, self.input.firstpersonflag = 1, 1
        self.assertEqual(self.choose(), 0)
        self.input.firstpersonflag = 0
        self.memory.put(BASE + INTERACTABLE, 0x3000000000)
        self.assertEqual(self.choose(), 0)  # low eax is zero, not whole pointer
        self.assertEqual(self.state.paperscanpickuppop, 0)

    def test_interaction_flag_only_starts_latch_and_static_zero_ends_it(self):
        self.input.interactableflag = 0
        self.assertEqual(self.choose(), 0)
        self.input.interactableflag = 1
        self.assertEqual(self.choose(), 1)
        self.input.interactableflag = 0
        self.assertEqual(self.choose(), 1)
        self.memory.put(BASE + INTERACTABLE, 0)
        self.assertEqual(self.choose(), 0)
        self.assertEqual(self.state.paperscanpickuppop, 0)
        self.assertEqual(self.state.pvmainadd, TARGET)  # reset at NEXT call entry
        self.assertEqual(self.choose(), 0)
        self.assertEqual(self.state.pvmainadd, 0)

    def test_failed_paper_chain_is_sentinel_and_cannot_reuse_partial_register(self):
        root = 0x3000000100 + INTERACTABLE * 16
        self.memory.put(root + 0x98, TARGET)
        self.assertEqual(self.choose(), 0)
        self.assertEqual(self.state.pvmainadd, BAD)
        self.memory.paper()
        self.assertEqual(self.choose(), 0)  # original sticky failure during pickup
        self.state.paperscanpickuppop = 0
        self.assertEqual(self.choose(), 1)

    def test_original_hex_minimum_and_failed_refresh_fail_closed(self):
        self.memory.paper(0xFFFFFFFB0)
        self.assertEqual(self.choose(0xFFFFFFFB0), 0)
        self.assertEqual(self.state.pvmainadd, BAD)
        self.state = State()
        self.memory.paper()
        self.assertEqual(self.choose(), 1)
        root = 0x3000000100 + INTERACTABLE * 16
        del self.memory.data[root + 0x98]
        self.assertEqual(self.choose(), 0)
        self.assertEqual(self.state.pvmainadd, TARGET)

    def test_target_replacement_only_refreshes_on_cached_target_then_reuses_new(self):
        replacement = TARGET + 0x10000
        self.assertEqual(self.choose(), 1)
        self.memory.paper(replacement)
        self.assertEqual(self.choose(replacement), 0)
        self.assertEqual(self.state.pvmainadd, TARGET)
        self.assertEqual(self.choose(), 0)
        self.assertEqual(self.state.pvmainadd, replacement)
        self.assertEqual(self.choose(replacement), 1)

    def test_scrap_exclusion_only_blocks_paper_in_eoz_inventory(self):
        self.input.scrapexamine, self.input.loadarea = 1, 37
        self.input.invflagadd = 0x3500000000
        self.memory.put(self.input.invflagadd, 1, 1)
        self.assertEqual(self.choose(), 0)
        self.memory.put(self.input.invflagadd, 0, 1)
        self.assertEqual(self.choose(), 1)
        self.memory.put(self.input.invflagadd, 1, 1)
        self.input.loadarea = 35
        self.assertEqual(self.choose(), 1)

    def test_secondary_only_matches_r13_with_lockbox_one_or_five(self):
        self.input.fixpaperscan = 0
        self.input.secintadd = 0x3200000040
        for box in range(7):
            self.input.lockboxflag = box
            self.assertEqual(self.choose(r13=self.input.secintadd),
                             2 if box in (1, 5) else 0)
            self.assertEqual(self.choose(r13=self.input.secintadd + 8), 0)
        self.input.lockboxflag, self.input.secintadd = 1, 0
        self.assertEqual(self.choose(r13=0), 2)  # no speculative nonzero restriction

    def test_prompt_producer_only_refreshes_when_interactable_flag_exactly_one(self):
        self.prompt_seen(C.byref(self.state), 0)
        self.assertEqual(self.state.activefprompt, 0)
        self.prompt_seen(C.byref(self.state), 1)
        self.assertEqual(self.state.activefprompt, 3)
        self.state.activefprompt = 1
        self.prompt_seen(C.byref(self.state), 2)
        self.assertEqual(self.state.activefprompt, 1)

    def test_prompt_ttl_decrements_once_per_changed_frame_only_if_path_reached(self):
        self.prompt_only()
        self.assertEqual(self.choose(), 3)
        self.input.frametrack = 1
        self.assertEqual(self.choose(), 3)
        self.assertEqual(self.choose(), 3)
        self.assertEqual(self.state.activefprompt, 2)
        self.input.frametrack = 27
        self.assertEqual(self.choose(), 3)
        self.input.frametrack = 28
        self.assertEqual(self.choose(), 0)
        self.assertEqual(self.state.activefprompt, 0)
        self.input.fixpaperscan = 1
        self.state.activefprompt, self.input.frametrack = 3, 29
        self.assertEqual(self.choose(), 1)
        self.assertEqual(self.state.activefprompt, 3)

    def test_prompt_exact_chain_and_rbx_identity_fail_closed_on_missing_link(self):
        self.prompt_only()
        self.assertEqual(self.choose(TARGET + 4), 0)
        self.assertEqual(self.choose(), 3)
        root = 0x3000000100 + FPROMPT * 16
        self.memory.put(root + 0x18, TARGET)
        self.assertEqual(self.choose(), 0)

    def test_prompt_notes_and_lockboxes_follow_original_cases(self):
        self.prompt_only()
        for ptr in (0, BAD, 0xFF):
            self.state.ivmainadd = ptr
            self.assertEqual(self.choose(), 0)
            self.assertEqual(self.state.readingnote, 1)
        self.state.ivmainadd = 0x100
        self.assertEqual(self.choose(), 3)
        self.assertEqual(self.state.readingnote, 0)
        self.input.lockboxflag = 1
        self.assertEqual(self.choose(), 0)
        self.input.lockboxflag = 2
        self.memory.popup(0)
        self.assertEqual(self.choose(), 0)
        self.memory.popup(2)
        self.assertEqual(self.choose(), 3)
        self.memory.popup(0x100)  # original AX/word, not a byte
        self.assertEqual(self.choose(), 3)

    def test_prompt_note_guard_compares_low_dword_as_unqualified_ct_cmp(self):
        self.prompt_only()
        self.state.ivmainadd = 0x21000000FF
        self.assertEqual(self.choose(), 0)
        self.assertEqual(self.state.readingnote, 1)
        self.state.ivmainadd = 0x2100000100
        self.assertEqual(self.choose(), 3)

    def test_herb_status_sticks_until_loadscreen_and_failed_popup_is_not_a_match(self):
        self.prompt_only()
        self.input.lockboxflag = 5
        self.assertEqual(self.choose(), 0)
        self.memory.popup(2)
        self.assertEqual(self.choose(), 0)
        self.memory.popup(0)
        self.assertEqual(self.choose(), 3)
        self.assertEqual(self.state.herbinwormbox, 1)
        self.memory.popup(2)
        self.assertEqual(self.choose(), 3)
        self.loadscreen(C.byref(self.state), 0)
        self.assertEqual(self.choose(), 3)
        self.loadscreen(C.byref(self.state), 1)
        self.assertEqual(self.state.herbinwormbox, 0)
        self.assertEqual(self.choose(), 0)

    def test_bedroom_gate_uses_unsigned_float_bits(self):
        self.prompt_only()
        self.input.loadarea = 29
        for value, expected in [(7.69, 3), (7.7, 0), (8, 0), (-1, 0)]:
            self.input.cplayerz = value
            self.assertEqual(self.choose(), expected)
        self.input.loadarea = 5
        self.assertEqual(self.choose(), 3)

    def test_main_item_tracker_uses_distinct_chain_and_rbx_plus_34(self):
        self.memory.main()
        self.assertEqual(self.track(TARGET + 4), 0)
        self.assertEqual(self.state.ivmainadd, TARGET)
        self.assertEqual(self.track(), 1)
        self.assertEqual(self.state.pvmainadd, 0)
        self.input.interactableflag = 0
        self.assertEqual(self.track(), 1)
        self.memory.put(BASE + INTERACTABLE, 0)
        self.assertEqual(self.track(), 0)
        self.assertEqual(self.state.itempickuppop, 0)
        self.assertEqual(self.track(), 0)
        self.assertEqual(self.state.ivmainadd, 0)

    def test_main_item_failure_sentinel_and_replacement(self):
        self.assertEqual(self.track(), 0)
        self.assertEqual(self.state.ivmainadd, BAD)
        self.memory.main()
        self.assertEqual(self.track(), 0)
        self.state.itempickuppop = 0
        self.assertEqual(self.track(), 1)
        replacement = TARGET + 0x1000
        self.memory.main(replacement)
        self.assertEqual(self.track(replacement), 0)
        self.assertEqual(self.track(), 0)
        self.assertEqual(self.track(replacement), 1)

    def test_auxiliary_secondary_inventory_and_scrap_producers(self):
        out = C.c_uint64(123)
        rax, rbx, name = 0x4100000000, 0x4200000000, 0x4300000000
        self.memory.put(rax + 0x18, TARGET)
        self.assertEqual(self.secondary(rax, self.memory.callback, None, C.byref(out)), 1)
        self.assertEqual(out.value, TARGET)
        self.assertEqual(self.secondary(rax + 8, self.memory.callback, None, C.byref(out)), 0)
        self.assertEqual(out.value, 0)
        self.memory.chain(INVENTORY, [0x208, 0x58, 0x148, 0x30, 0x48], name)
        self.assertEqual(self.inventory(BASE, self.memory.callback, None, C.byref(out)), 1)
        self.assertEqual(out.value, name + 0x10)
        self.memory.put(rbx + 0x78, name)
        for offset, ch in [(0x14, 'N'), (0x26, '0'), (0x28, '9')]:
            self.memory.put(name + offset, ord(ch), 2)
        self.assertEqual(self.scrap(rbx, self.memory.callback, None), 1)
        self.memory.put(name + 0x28, ord('8'), 2)
        self.assertEqual(self.scrap(rbx, self.memory.callback, None), 0)
        self.assertEqual(self.scrap(rbx + 8, self.memory.callback, None), 0)

    def test_original_frame_wrap_and_location_producers(self):
        self.assertEqual([self.timer(n) for n in (0, 999, 1000, 1001)], [1, 1000, 1001, 1])
        for area, x, y, z, expected in [
            (5, 28, 0, 12, 1), (5, -11, 0, 0, 1),
            (5, -26, 0, 85, 5), (5, -50, 0, 86, 1),
            (5, -9, 0, 3, 1), (5, -16, 0, 8, 1),
            (5, 22, 0, 3, 2), (5, -12, 4, 0, 2),
            (5, -16, 4, 85, 3), (5, 31, 0, 0, 2),
            (6, 61, 28, 29, 1), (6, 61, 26, 29, 0),
            (4, 28, 0, 12, 0), (5, 0, 0, 0, 0),
        ]:
            with self.subTest(area=area, xyz=(x, y, z)):
                self.assertEqual(self.lockbox(area, x, y, z), expected)


if __name__ == '__main__':
    unittest.main()
