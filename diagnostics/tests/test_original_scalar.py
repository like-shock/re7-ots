"""Behavior tests for the original CT port, not the Explore Lua camera.

Would catch: aim forcing FP, loss of recent-FP, reversed shoulder/axis signs,
collapsed x87 stores, X using Z's new multipliers, clamped focus overshoot,
float comparisons replacing CT's integer comparisons, stale firing coordinates.
"""
import ctypes as C
import math
import os
from pathlib import Path
import random
import struct
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
PARAM_NAMES = ('backdisadjust behinddis aimbehinddis camclipbehinddis pitchmul '
               'sidedisadjust sidedis aimsidedis camclipsidedis heightdis '
               'heightdisadjust camclipheightdis yadd headadjust sideaimadjust '
               'firstyadd tppitchadj').split()

class Params(C.Structure):
    _fields_ = [(n, C.c_float) for n in PARAM_NAMES]

class ViewInput(C.Structure):
    _fields_ = [(n, C.c_uint8) for n in
                ('stopflag', 'internalfpflag', 'thirdpersonflag', 'fpaimflag', 'aim')]

class ViewState(C.Structure):
    _fields_ = [('firstpersonflag', C.c_uint8), ('recentfpflag', C.c_uint8)]

class FocusState(C.Structure):
    _fields_ = [('unzoomflag', C.c_uint8), ('unzoomsidedoneflag', C.c_uint8)]

class FocusRates(C.Structure):
    _fields_ = [(n, C.c_float) for n in
                ('aimtick', 'sideaimtick', 'heighttick', 'minaimbehinddis', 'maxaimsidedis')]

class Vec(C.Structure):
    _fields_ = [(n, C.c_float) for n in ('x', 'y', 'z')]


def bits(value):
    return struct.pack('<f', value)


class OriginalScalarTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.tmp.cleanup)
        so = Path(cls.tmp.name) / 'original.so'
        subprocess.run(['cc', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                        '-ffp-contract=off', '-fno-fast-math', '-shared', '-fPIC',
                        str(ROOT/'src/original_scalar.c'), '-o', str(so)], check=True)
        cls.lib = lib = C.CDLL(str(so))
        declarations = {
            'defaults': ([C.POINTER(Params)], None),
            'view': ([C.POINTER(ViewInput), C.POINTER(ViewState)], None),
            'ots_axis': ([C.c_uint, C.c_float, C.c_float, C.c_float, C.c_float,
                          C.POINTER(Params)], C.c_float),
            'fp_axis': ([C.c_uint, C.c_float, C.c_float, C.c_float,
                         C.POINTER(Params)], C.c_float),
            'pitch_multiplier': ([C.c_float], C.c_float),
            'adjust_pitch': ([C.c_float, C.c_float], C.c_float),
            'focus_step': ([C.POINTER(Params), C.POINTER(FocusState), C.POINTER(FocusRates),
                            C.c_uint, C.c_uint8, C.c_uint8], None),
            'water_y': ([C.c_float, C.c_uint32, Vec], C.c_float),
            'firing_origin': ([Vec, Vec, C.c_int], Vec),
        }
        for name, (args, result) in declarations.items():
            fn = getattr(lib, 're7_original_' + name)
            fn.argtypes, fn.restype = args, result
            setattr(cls, name, staticmethod(fn))

    def params(self):
        p = Params()
        self.defaults(C.byref(p))
        return p

    def test_original_defaults_and_default_aim_stay_third_person(self):
        p = self.params()
        self.assertAlmostEqual(p.backdisadjust, 1.3)
        self.assertAlmostEqual(p.sidedisadjust, .3)
        self.assertAlmostEqual(p.heightdisadjust, .9)
        self.assertEqual(p.pitchmul, 0)  # CT initialization; first Z write computes it.
        self.assertEqual((p.headadjust, p.sideaimadjust, p.firstyadd), (0, 0, 0))
        s = ViewState()
        self.view(C.byref(ViewInput(0, 0, 1, 0, 1)), C.byref(s))
        self.assertEqual((s.firstpersonflag, s.recentfpflag), (0, 0))

    def test_view_truth_table_and_sticky_recent_flag_without_model(self):
        for mask in range(32):
            stop, internal, third, fp_aim, aim = [(mask >> b) & 1 for b in range(5)]
            s = ViewState()
            self.view(C.byref(ViewInput(stop, internal, third, fp_aim, aim)), C.byref(s))
            expected = int(stop or internal or not third or (fp_aim and aim))
            self.assertEqual((s.firstpersonflag, s.recentfpflag), (expected, expected))
        self.view(C.byref(ViewInput(0, 0, 1, 0, 0)), C.byref(s))
        self.assertEqual((s.firstpersonflag, s.recentfpflag), (0, 1))
        # Literal CT byte comparisons: stop/internal require 1, third requires 0,
        # while HEAD_Y_WRITE treats every nonzero fpaim value as enabled.
        self.view(C.byref(ViewInput(2, 2, 2, 2, 1)), C.byref(s))
        self.assertEqual(s.firstpersonflag, 1)
        self.view(C.byref(ViewInput(2, 2, 2, 2, 2)), C.byref(s))
        self.assertEqual(s.firstpersonflag, 0)

    def test_axes_both_shoulders_and_wall_contraction(self):
        p = self.params()
        p.pitchmul = 1
        # yaw=(0,1): behind is positive Z; shoulder is positive X.
        self.assertAlmostEqual(self.ots_axis(0, 10, 0, 1, 0, C.byref(p)), 10.3, places=5)
        self.assertAlmostEqual(self.ots_axis(2, 30, 0, 1, 0, C.byref(p)), 31.3, places=5)
        p.sidedis = -1
        self.assertAlmostEqual(self.ots_axis(0, 10, 0, 1, 0, C.byref(p)), 9.7, places=5)
        p.camclipbehinddis = .3
        p.camclipsidedis = .7
        p.camclipheightdis = .5
        self.assertAlmostEqual(self.ots_axis(0, 10, 0, 1, .5, C.byref(p)), 9.79, places=5)
        self.assertAlmostEqual(self.ots_axis(1, 20, 0, 1, .5, C.byref(p)), 20.225, places=5)
        self.assertAlmostEqual(self.ots_axis(2, 30, 0, 1, .5, C.byref(p)), 30.39, places=5)
        s = ViewState()
        self.view(C.byref(ViewInput(0, 0, 1, 0, 0)), C.byref(s))
        self.assertEqual(s.firstpersonflag, 0)

    def test_pitch_and_focus_update_can_stay_between_x_and_z(self):
        p = self.params()
        p.pitchmul = 1
        x = self.ots_axis(0, 10, .6, .8, .5, C.byref(p))
        p.pitchmul = self.pitch_multiplier(.5)
        p.aimbehinddis = .5
        z = self.ots_axis(2, 30, .6, .8, .5, C.byref(p))
        self.assertAlmostEqual(x, 9.46, places=5)
        self.assertAlmostEqual(z, 30.544, places=5)
        self.assertEqual(bits(p.pitchmul), bits(self.pitch_multiplier(-.5)))
        self.assertAlmostEqual(self.adjust_pitch(.5, .125), .375)
        self.assertLess(self.pitch_multiplier(2), 0)  # Original does not clamp.

    def test_fp_offsets_do_not_require_head_hiding(self):
        p = self.params()
        p.pitchmul = .75
        for axis in range(3):
            self.assertEqual(self.fp_axis(axis, 10, .6, .8, C.byref(p)), 10)
        p.headadjust, p.sideaimadjust, p.firstyadd = .2, -.1, .15
        values = [self.fp_axis(i, 10, .6, .8, C.byref(p)) for i in range(3)]
        for actual, expected in zip(values, (10.01, 10.15, 9.82)):
            self.assertAlmostEqual(actual, expected, places=5)

    def test_interactable_expression_reuses_current_camera_multipliers(self):
        p = self.params()
        p.pitchmul = .8
        p.aimbehinddis = .45
        p.aimsidedis = 1.3
        before = bytes(p)
        # Same displacement on a separately identified inspection transform;
        # this is not evidence that all HUD prompts use that transform.
        for axis in range(3):
            a = self.ots_axis(axis, 5, .6, -.8, -.5, C.byref(p))
            b = self.ots_axis(axis, 15, .6, -.8, -.5, C.byref(p))
            self.assertAlmostEqual(b - a, 10, places=5)
        self.assertEqual(bytes(p), before)

    def test_focus_checks_gun_and_keeps_original_threshold_overshoot(self):
        p, s = self.params(), FocusState()
        rates = FocusRates(.1, .1, .1, .45, 1.3)
        before = bytes(p)
        self.focus_step(C.byref(p), C.byref(s), C.byref(rates), 1, 1, 0)
        self.assertEqual(bytes(p), before)
        self.assertEqual(s.unzoomflag, 0)
        p.aimbehinddis, p.aimsidedis, p.heightdisadjust = .45, 1.3, .31
        self.focus_step(C.byref(p), C.byref(s), C.byref(rates), 1, 1, 1)
        self.assertAlmostEqual(p.aimbehinddis, .35, places=6)
        self.assertAlmostEqual(p.aimsidedis, 1.4, places=6)
        self.assertAlmostEqual(p.heightdisadjust, .3, places=6)
        self.assertEqual((s.unzoomflag, s.unzoomsidedoneflag), (1, 0))

    def test_disabled_gun_check_jumps_over_height_and_flags(self):
        p, s = self.params(), FocusState(0, 1)
        rates = FocusRates(.1, .1, .1, .45, 1.3)
        p.heightdisadjust = .31
        self.focus_step(C.byref(p), C.byref(s), C.byref(rates), 1, 0, 0)
        self.assertAlmostEqual(p.heightdisadjust, .31, places=6)
        self.assertEqual((s.unzoomflag, s.unzoomsidedoneflag), (0, 1))
        self.assertAlmostEqual(p.aimbehinddis, .9, places=6)
        self.assertAlmostEqual(p.aimsidedis, 1.1, places=6)
        # A jump directly to nozoomguncheck is a DIFFERENT original entry.
        self.focus_step(C.byref(p), C.byref(s), C.byref(rates), 3, 1, 0)
        self.assertAlmostEqual(p.heightdisadjust, .3, places=6)
        self.assertEqual((s.unzoomflag, s.unzoomsidedoneflag), (1, 0))

    def test_unzoom_behind_done_skips_side_decrement_when_marking_done(self):
        p, s = self.params(), FocusState(1, 0)
        rates = FocusRates(.1, .1, .1, .45, 1.3)
        p.aimbehinddis, p.aimsidedis = 1, 1.3
        self.focus_step(C.byref(p), C.byref(s), C.byref(rates), 2, 1, 0)
        self.assertAlmostEqual(p.aimsidedis, 1.3, places=6)
        self.assertEqual((s.unzoomflag, s.unzoomsidedoneflag), (1, 1))
        self.focus_step(C.byref(p), C.byref(s), C.byref(rates), 2, 1, 0)
        self.assertAlmostEqual(p.aimsidedis, 1.2, places=6)
        self.assertEqual(s.unzoomflag, 0)

    def test_unzoom_completion_keeps_original_one_call_delay(self):
        p, s = self.params(), FocusState(1, 0)
        rates = FocusRates(.1, .1, .1, .45, 1.3)
        p.aimbehinddis, p.aimsidedis = .95, 1.05
        self.focus_step(C.byref(p), C.byref(s), C.byref(rates), 2, 1, 0)
        self.assertAlmostEqual(p.aimbehinddis, 1.05, places=6)
        self.assertAlmostEqual(p.aimsidedis, .95, places=6)
        self.assertEqual((s.unzoomflag, s.unzoomsidedoneflag), (1, 0))
        self.focus_step(C.byref(p), C.byref(s), C.byref(rates), 2, 1, 0)
        self.assertEqual((s.unzoomflag, s.unzoomsidedoneflag), (1, 1))
        self.focus_step(C.byref(p), C.byref(s), C.byref(rates), 2, 1, 0)
        self.assertEqual(s.unzoomflag, 0)
        self.assertAlmostEqual(p.heightdisadjust, .9, places=6)

    def test_focus_retains_unsigned_float_bit_comparisons(self):
        p, s = self.params(), FocusState()
        rates = FocusRates(.1, .1, 0, .45, 1.3)
        p.aimbehinddis, p.aimsidedis = -.5, -.5
        self.focus_step(C.byref(p), C.byref(s), C.byref(rates), 1, 0, 0)
        self.assertAlmostEqual(p.aimbehinddis, -.6, places=6)
        self.assertEqual(p.aimsidedis, -.5)

    def test_guest_house_water_clamp_boundaries(self):
        for xyz in ((18, -8, 18), (36.5, -8.8, 26), (20, -8.5, 20)):
            self.assertEqual(bits(self.water_y(-9, 4, Vec(*xyz))), bits(-7.15))
        for xyz in ((17.99, -8.5, 20), (36.51, -8.5, 20), (20, -7.99, 20),
                    (20, -8.81, 20), (20, -8.5, 17.99), (20, -8.5, 26.01)):
            self.assertEqual(self.water_y(-9, 4, Vec(*xyz)), -9)
        self.assertEqual(self.water_y(-9, 5, Vec(20, -8.5, 20)), -9)
        self.assertEqual(self.water_y(-7, 4, Vec(20, -8.5, 20)), -7)

    def test_firing_origin_all_coordinates_or_original_fallback(self):
        old, cam = Vec(1, 2, 3), Vec(4, 5, 6)
        result = self.firing_origin(old, cam, 1)
        self.assertEqual((result.x, result.y, result.z), (4, 5, 6))
        for candidate, available in ((cam, 0), (Vec(math.nan, 5, 6), 1),
                                     (Vec(4, math.inf, 6), 1), (Vec(4, 5, -math.inf), 1)):
            result = self.firing_origin(old, candidate, available)
            self.assertEqual((result.x, result.y, result.z), (1, 2, 3))

    def test_pinned_original_x87_arithmetic_oracle(self):
        path = Path(os.environ.get('RE7_CT_FILE', ROOT.parent/'original/re7.CT'))
        if not path.exists():
            self.skipTest('Private CT absent; set RE7_CT_FILE for original instruction comparison.')
        from original_ct_oracle import Oracle
        oracle = Oracle(path, Path(self.tmp.name))
        rng = random.Random(802)
        for _ in range(1000):
            p = self.params()
            for field in PARAM_NAMES:
                setattr(p, field, rng.uniform(-2, 2))
            base, yawx, yawz, pitch = [C.c_float(rng.uniform(-10, 10)).value for _ in range(4)]
            values = {n: getattr(p, n) for n in PARAM_NAMES}
            values.update(currentyawxbehind=yawx, currentyawxside=yawx,
                          currentyawzbehind=yawz, currentyawzside=yawz,
                          camxattempt=base, camyattempt=base, camzattempt=base,
                          currentpitch=pitch, fstorage=pitch, onefloat=1, pitchratio=C.c_float(.6).value)
            for axis, name in enumerate(('ots_x', 'ots_y', 'ots_z')):
                self.assertEqual(bits(self.ots_axis(axis, base, yawx, yawz, pitch, C.byref(p))),
                                 bits(oracle.run(name, values)))
            for axis, name in ((0, 'fp_x'), (2, 'fp_z')):
                self.assertEqual(bits(self.fp_axis(axis, base, yawx, yawz, C.byref(p))),
                                 bits(oracle.run(name, values)))
            self.assertEqual(bits(self.pitch_multiplier(pitch)), bits(oracle.run('pitchmul', values)))


if __name__ == '__main__':
    unittest.main()
