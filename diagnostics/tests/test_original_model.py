"""Original RT 8.02 transparency decisions, not Explore SDK mesh visibility.

CT lines 1260-1272 write byte 6 before the original DWORD read; lines
1285-1299 suppress the original QWORD write when culflag != 0 and the
interactable slot's low DWORD is zero. Tests catch inverted branch conditions,
wrong access width, stale decisions, broad writes, and unsafe target fallback.
"""
import ctypes as C
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
BASE = 0x140000000
OWNER = 0x234560000
INTERACT_RVA = 0x8FB17F0
Read = C.CFUNCTYPE(C.c_int, C.c_void_p, C.c_uint64, C.c_void_p, C.c_size_t)


class Input(C.Structure):
    _fields_ = [('image_base', C.c_uint64), ('owner', C.c_uint64),
                ('validated_hook', C.c_uint8), ('culflag', C.c_uint8)]


class Decision(C.Structure):
    _fields_ = [('byte_write_address', C.c_uint64),
                ('byte_write_value', C.c_uint8),
                ('suppress_original_write', C.c_uint8)]


class OriginalModelTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = ROOT / 'src/original_model.c'
        assert source.is_file(), 'The original native transparency adapter is missing'
        cls.tmp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.tmp.cleanup)
        so = Path(cls.tmp.name) / 'original_model.so'
        subprocess.run(['cc', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                        '-shared', '-fPIC', str(source), '-o', str(so)], check=True)
        cls.lib = C.CDLL(str(so))
        for name in ('player_read', 'gun_write'):
            fn = getattr(cls.lib, 're7_original_model_' + name)
            fn.argtypes = [Read, C.c_void_p, C.POINTER(Input), C.POINTER(Decision)]
            fn.restype = None
            setattr(cls, name, staticmethod(fn))

    def setUp(self):
        self.regions = {BASE + INTERACT_RVA: bytes(8),
                        OWNER + 0x3D0: struct.pack('<Q', 0xAABBCCDD11223344)}
        self.failed_addresses = set()
        self.reads = []
        self.input = Input(BASE, OWNER, 1, 1)  # original culflag default is one

        @Read
        def read(_ctx, address, dest, size):
            self.reads.append((address, size))
            if address in self.failed_addresses:
                return 0
            data = self.regions.get(address)
            if data is None or len(data) < size:
                return 0
            C.memmove(dest, data, size)
            return 1
        self.read = read

    def decision(self, operation):
        # Poison previous output to verify that a failed/current mode decision
        # never leaves an old game-memory write scheduled.
        out = Decision(0xDEADBEEF, 0xFF, 1)
        getattr(self, operation)(self.read, None, C.byref(self.input), C.byref(out))
        return out.byte_write_address, out.byte_write_value, out.suppress_original_write

    def test_original_read_changes_only_second_flag_byte(self):
        self.assertEqual(self.decision('player_read'), (OWNER + 0x3D1, 6, 0))
        self.assertEqual(self.regions[OWNER + 0x3D0], struct.pack('<Q', 0xAABBCCDD11223344))
        self.assertEqual(self.reads, [(BASE + INTERACT_RVA, 4), (OWNER + 0x3D0, 4)])

    def test_original_gun_suppresses_only_original_store(self):
        self.assertEqual(self.decision('gun_write'), (0, 0, 1))
        self.assertEqual(self.reads, [(BASE + INTERACT_RVA, 4), (OWNER + 0x3D0, 8)])

    def test_interaction_uses_low_dword_not_pointer_truthiness(self):
        self.regions[BASE + INTERACT_RVA] = struct.pack('<Q', 0xDEADBEEF00000000)
        self.assertEqual(self.decision('player_read'), (OWNER + 0x3D1, 6, 0))
        self.regions[BASE + INTERACT_RVA] = struct.pack('<Q', 0x10000)
        self.assertEqual(self.decision('player_read'), (0, 0, 0))
        self.assertEqual(self.decision('gun_write'), (0, 0, 0))

    def test_culflag_zero_disables_and_every_nonzero_byte_enables(self):
        for value in (0, 1, 2, 255, 0):
            self.input.culflag = value
            self.assertEqual(self.decision('gun_write'), (0, 0, int(value != 0)))

    def test_interaction_changes_are_not_cached(self):
        for value in (0, 1, 0xFFFFFFFF, 0):
            self.regions[BASE + INTERACT_RVA] = struct.pack('<I', value)
            want = (OWNER + 0x3D1, 6, 0) if value == 0 else (0, 0, 0)
            self.assertEqual(self.decision('player_read'), want)
            self.assertEqual(self.decision('gun_write'), (0, 0, int(value == 0)))

    def test_false_hook_target_never_reads_or_requests_mutation(self):
        for validated in (0, 2, 255):
            self.input.validated_hook = validated
            self.assertEqual(self.decision('player_read'), (0, 0, 0))
            self.assertEqual(self.decision('gun_write'), (0, 0, 0))
        self.assertEqual(self.reads, [])

    def test_failed_reads_restore_original_behavior_immediately(self):
        for location in (BASE + INTERACT_RVA, OWNER + 0x3D0):
            self.assertEqual(self.decision('gun_write'), (0, 0, 1))
            self.failed_addresses.add(location)
            self.assertEqual(self.decision('player_read'), (0, 0, 0))
            self.assertEqual(self.decision('gun_write'), (0, 0, 0))
            self.failed_addresses.clear()

    def test_invalid_owner_never_produces_address_or_skip(self):
        for owner in (0, 0xFFFF, 0x7FFFFFFFFFFF - 0x3D3,
                      0x800000000000, 0xFFFFFFFFFFFFFF00):
            self.input.owner = owner
            self.assertEqual(self.decision('player_read'), (0, 0, 0))
            self.assertEqual(self.decision('gun_write'), (0, 0, 0))

    def test_invalid_image_never_reads_or_mutates(self):
        for base in (0, 0xFFFF, 0x7FFFFFFFFFFF - INTERACT_RVA - 1,
                     0xFFFFFFFFFFFFFF00):
            self.input.image_base = base
            self.assertEqual(self.decision('player_read'), (0, 0, 0))
            self.assertEqual(self.decision('gun_write'), (0, 0, 0))
        self.assertEqual(self.reads, [])

    def test_null_dependency_clears_previous_decision(self):
        null_read = Read()
        for fn in (self.player_read, self.gun_write):
            for read, inp in ((null_read, C.byref(self.input)), (self.read, None)):
                out = Decision(OWNER + 0x3D1, 6, 1)
                fn(read, None, inp, C.byref(out))
                self.assertEqual((out.byte_write_address, out.byte_write_value,
                                  out.suppress_original_write), (0, 0, 0))
            fn(self.read, None, C.byref(self.input), None)


if __name__ == '__main__':
    unittest.main()
