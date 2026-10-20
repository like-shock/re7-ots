"""Source-derived RT 8.02 DOF collision tests, independent of Explore raycasts.

Catches: center-ray substitution, collapsed one-call angle state, reversed shoulder
signs, ignoring the 0.001 deadzone or 0.6 zoom-out bracket, FP proximity fallback,
forgetting the 3-read delay, and nonfinite values entering the native camera.
The optional private-CT oracle executes the original multiplier instructions;
it never copies the user's CT into the repository or published artifacts.
"""
import ctypes as C
import hashlib
import math
import os
from pathlib import Path
import random
import re
import struct
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
PARAM_NAMES = ('backdisadjust behinddis aimbehinddis camclipbehinddis pitchmul '
               'sidedisadjust sidedis aimsidedis camclipsidedis heightdis '
               'heightdisadjust camclipheightdis yadd headadjust sideaimadjust '
               'firstyadd tppitchadj').split()
CONFIG_NAMES = ('camclipstartmult camclipdegreeadjust clipzoomintickrate '
                'clipzoomouttickrate cczoomoutbuffer camclipbackminmult '
                'camclipsideminmult').split()
STATE_FLOATS = ('rawdofattempt dofattempt viewdofattempt camtrigangle '
                'camclipangleadjust oldwallping').split()

class Params(C.Structure):
    _fields_ = [(n, C.c_float) for n in PARAM_NAMES]

class Config(C.Structure):
    _fields_ = [(n, C.c_float) for n in CONFIG_NAMES]

class Flags(C.Structure):
    _fields_ = [(n, C.c_uint8) for n in
                ('firstpersonflag', 'tpcamclip', 'noclipautodisable', 'stopdofflag')]

class State(C.Structure):
    _fields_ = [(n, C.c_float) for n in STATE_FLOATS] + [
        ('rdofdelayframes', C.c_uint32), ('camiswest', C.c_uint8),
        ('camisnorth', C.c_uint8), ('bypassclipdeadzone', C.c_uint8),
        ('sample_valid', C.c_uint8)]

class Vec(C.Structure):
    _fields_ = [(n, C.c_float) for n in ('x', 'y', 'z')]

class Read(C.Structure):
    _fields_ = [('flags', Flags), ('distance', C.c_float), ('loadarea', C.c_uint32),
                ('playerx', C.c_float), ('playerz', C.c_float), ('available', C.c_uint8)]

class Input(C.Structure):
    _fields_ = [('flags', Flags), ('direction', Vec), ('camera_attempt', Vec),
                ('yawx', C.c_float), ('yawz', C.c_float), ('framerate', C.c_float),
                ('autooldcamfov', C.c_float), ('available', C.c_uint8)]

class Output(C.Structure):
    _fields_ = [('direction', Vec), ('status', C.c_uint32)]


def f32(v):
    return C.c_float(v).value


def bits(v):
    return struct.pack('<f', v)


class CollisionOracle:
    """Run the pinned original Normal DOF block after resolving game loads.

    Retains original x87 instructions, named/anonymous branches, unsigned CMPs,
    temporary stores, and register outputs. Only replaces external pointers with
    scalar cells; the source is read privately and generated assembly is temporary.
    """
    def __init__(self, path, output):
        raw = path.read_bytes()
        expected = '4154ee807842eb17f51b84272c98c3fdf153082c252f176446aa0cd61fd0686d'
        if hashlib.sha256(raw).hexdigest() != expected:
            raise ValueError('Original CT hash does not match RT 8.02 reference')
        lines = raw.decode('utf-8-sig').splitlines()[6487:7032]
        ops = []
        skip_r8_pointer = False
        for line in lines:
            op = line.split('//')[0].strip()
            if not op or op.startswith('{$'):
                continue
            if skip_r8_pointer:
                if op != 'mov r8,[r8]':
                    raise ValueError('Unexpected camera pointer sequence')
                skip_r8_pointer = False
                continue
            if op in ('mov r8,[yawx]', 'mov r8,[yawz]'):
                op = op.replace('mov r8,', 'mov r8d,')
                skip_r8_pointer = True
            for address, symbol in (('rdi+00000560', 'outputx'),
                                    ('rdi+00000564', 'outputy'),
                                    ('rdi+00000568', 'outputz'),
                                    ('rax+04', 'sourcey'), ('rax+08', 'sourcez')):
                op = op.replace('[' + address + ']', '[' + symbol + ']')
            op = op.replace('push eax', 'push rax').replace('pop eax', 'pop rax')
            ops.append(op)
        self.fields = ['sourcex', 'sourcey', 'sourcez', 'finalecx', 'finaleax']
        for op in ops:
            for field in re.findall(r'\[(\w+)\]', op):
                if field not in self.fields:
                    self.fields.append(field)
        self.offsets = {field: 4 * n for n, field in enumerate(self.fields)}
        anonymous = [n for n, op in enumerate(ops) if op == '@@:']
        asm = ['.intel_syntax noprefix', '.text', '.globl oracle_collision',
               'oracle_collision:', f'mov ecx, DWORD PTR [rdi+{self.offsets["sourcex"]}]']
        for n, op in enumerate(ops):
            if op == '@@:':
                asm.append(f'.Lanon{n}:')
                continue
            if op.endswith(':'):
                asm.append('.Lct_' + op)
                continue
            if '@f' in op:
                op = op.replace('@f', '.Lanon' + str(next(x for x in anonymous if x > n)))
            jump = re.fullmatch(r'(j\w+) (\w+)', op)
            if jump:
                target = jump[2]
                target = '.Ldone' if target in ('return5', 'return6', 'return7', 'code6', 'code7') else '.Lct_' + target
                op = jump[1] + ' ' + target
            op = re.sub(r'\(float\)(-?[0-9.]+)',
                        lambda m: hex(struct.unpack('<I', bits(float(m[1])))[0]), op)
            op = op.replace(',80000000', ',0x80000000')
            op = re.sub(r'\[(\w+)\]', lambda m: '[rdi+' + str(self.offsets[m[1]]) + ']', op)
            if 'ptr [' not in op:
                op = op.replace('[rdi+', 'DWORD PTR [rdi+')
            asm.append(op)
        asm += ['.Ldone:', f'mov DWORD PTR [rdi+{self.offsets["finalecx"]}], ecx',
                f'mov DWORD PTR [rdi+{self.offsets["finaleax"]}], eax', 'ret',
                '.section .note.GNU-stack,"",@progbits']
        source, library = output/'collision_oracle.S', output/'collision_oracle.so'
        source.write_text('\n'.join(asm) + '\n')
        subprocess.run(['cc', '-shared', '-fPIC', str(source), '-o', str(library)], check=True)
        self.lib = C.CDLL(str(library))
        self.lib.oracle_collision.argtypes = [C.POINTER(C.c_uint32)]
        self.lib.oracle_collision.restype = None

    def run(self, values):
        integer_fields = {'firstpersonflag', 'tpcamclip', 'noclipautodisable', 'stopdofflag',
                          'camiswest', 'camisnorth', 'bypassclipdeadzone', 'dofangleopcode'}
        cells = (C.c_uint32 * len(self.fields))()
        for field, value in values.items():
            if field in self.fields:
                cells[self.fields.index(field)] = (int(value) if field in integer_fields else
                                                    struct.unpack('<I', bits(value))[0])
        self.lib.oracle_collision(cells)
        return {field: (cells[n] if field in integer_fields else
                        struct.unpack('<f', struct.pack('<I', cells[n]))[0])
                for n, field in enumerate(self.fields)}


class OriginalCollisionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.tmp.cleanup)
        cls.lib = None
        if not (ROOT/'src/original_collision.c').is_file():
            return
        so = Path(cls.tmp.name)/'collision.so'
        subprocess.run(['cc', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                        '-ffp-contract=off', '-fno-fast-math', '-shared', '-fPIC',
                        str(ROOT/'src/original_scalar.c'), str(ROOT/'src/original_collision.c'),
                        '-o', str(so)], check=True)
        cls.lib = lib = C.CDLL(str(so))
        declarations = {
            'defaults': ([C.POINTER(Config), C.POINTER(State)], None),
            'distance_read': ([C.POINTER(State), C.POINTER(Read)], C.c_int),
            'step': ([C.POINTER(Params), C.POINTER(Config), C.POINTER(State),
                      C.POINTER(Input)], Output),
            'framerate': ([C.c_float, C.c_float, C.c_float], C.c_float),
        }
        for name, (args, result) in declarations.items():
            fn = getattr(lib, 're7_original_collision_' + name)
            fn.argtypes, fn.restype = args, result
            setattr(cls, name, staticmethod(fn))
        cls.examine_write = staticmethod(getattr(lib, 're7_original_collision_examine_write', None))
        if cls.examine_write is not None:
            cls.examine_write.argtypes = [C.POINTER(State), C.POINTER(Flags), C.c_float, C.c_int]
            cls.examine_write.restype = C.c_int
        lib.re7_original_defaults.argtypes = [C.POINTER(Params)]
        lib.re7_original_defaults.restype = None

    def setup(self):
        self.assertIsNotNone(self.lib, 'original DOF collision core is not implemented')
        p, c, s = Params(), Config(), State()
        self.lib.re7_original_defaults(C.byref(p))
        self.defaults(C.byref(c), C.byref(s))
        p.pitchmul = 1
        i = Input(Flags(0, 1, 0, 1), Vec(0, .2, 1), Vec(10, 20, 30), 0, 1, 60, 80, 1)
        return p, c, s, i

    def run_step(self, p, c, s, i):
        return self.step(C.byref(p), C.byref(c), C.byref(s), C.byref(i))

    def read(self, s, distance, flags=None, **kw):
        r = Read(flags or Flags(0, 1, 0, 1), distance, kw.get('loadarea', 0),
                 kw.get('playerx', 0), kw.get('playerz', 0), kw.get('available', 1))
        return self.distance_read(C.byref(s), C.byref(r))

    def test_original_defaults_and_timer_rounding(self):
        _, c, s, _ = self.setup()
        self.assertEqual([bits(getattr(c, n)) for n in CONFIG_NAMES],
                         [bits(x) for x in (1.5, 10, 0, 1, .6, .3, .7)])
        self.assertEqual((s.dofattempt, s.oldwallping, s.camtrigangle, s.camclipangleadjust),
                         (0, 0, 0, 0))
        self.assertEqual(s.sample_valid, 1)
        self.assertEqual(self.framerate(60, f32(1/59.4), 1), 59)
        self.assertEqual(self.framerate(60, f32(1/20), 1), 28)
        self.assertEqual(self.framerate(73, .02, 0), 73)
        self.assertEqual(self.framerate(60, .02, .5), 28)
        self.assertEqual(self.framerate(60, 0, 1), 60)  # intentional validation

    def test_wall_distance_uses_ratio_with_independent_original_minima(self):
        p, c, s, i = self.setup()
        self.read(s, .75)
        o = self.run_step(p, c, s, i)
        self.assertEqual(o.status, 1)
        self.assertEqual((p.camclipbehinddis, p.camclipheightdis), (.5, .5))
        self.assertEqual(bits(p.camclipsidedis), bits(.7))
        self.assertEqual(s.oldwallping, .75)
        self.assertEqual(i.flags.firstpersonflag, 0)
        self.read(s, .01)
        self.run_step(p, c, s, i)
        self.assertEqual(bits(p.camclipbehinddis), bits(.3))
        self.assertEqual(bits(p.camclipsidedis), bits(.7))
        self.assertEqual(bits(p.camclipheightdis), bits(.3))

    def test_deadzone_leaves_old_ping_and_multipliers_untouched(self):
        p, c, s, i = self.setup()
        self.read(s, .75)
        self.run_step(p, c, s, i)
        before = (p.camclipbehinddis, p.camclipsidedis, p.camclipheightdis)
        self.read(s, .7505)
        self.run_step(p, c, s, i)
        self.assertEqual((p.camclipbehinddis, p.camclipsidedis, p.camclipheightdis), before)
        self.assertEqual(s.oldwallping, .75)
        self.read(s, .74)
        self.run_step(p, c, s, i)
        self.assertLess(p.camclipbehinddis, .5)

    def test_slow_zoom_bypasses_deadzone_until_target_and_handles_negative_tick(self):
        p, c, s, i = self.setup()
        c.clipzoomintickrate = 1
        self.read(s, .75)
        self.run_step(p, c, s, i)
        first = p.camclipbehinddis
        self.assertAlmostEqual(first, 1 - 1/60, places=6)
        self.assertEqual(s.bypassclipdeadzone, 1)
        self.run_step(p, c, s, i)
        self.assertLess(p.camclipbehinddis, first)
        # Original unsigned compare first permits negative subtraction, then
        # the explicit sign-bit guards choose instantclipmult for all axes.
        c.clipzoomintickrate = .0001
        self.run_step(p, c, s, i)
        self.assertEqual(p.camclipbehinddis, .5)
        self.assertEqual(s.bypassclipdeadzone, 0)

    def test_zoom_out_buffer_holds_then_allows_return_and_fov25_bypasses(self):
        p, c, s, i = self.setup()
        p.camclipbehinddis = p.camclipheightdis = .5
        p.camclipsidedis = .7
        self.read(s, 1)
        self.run_step(p, c, s, i)
        self.assertEqual(p.camclipbehinddis, .5)  # bracket floor .45 < next .775
        self.assertEqual(s.oldwallping, 1)
        self.read(s, 1.2)
        self.run_step(p, c, s, i)
        self.assertAlmostEqual(p.camclipbehinddis, .5 + 1/60, places=6)
        self.read(s, 1.49)
        i.autooldcamfov = 25
        p.camclipbehinddis = .8
        self.run_step(p, c, s, i)
        self.assertAlmostEqual(p.camclipbehinddis, .8 + 1/60, places=6)
        self.read(s, 50)
        p.camclipbehinddis = .98
        self.run_step(p, c, s, i)
        self.assertEqual(p.camclipbehinddis, 1)
        c.clipzoomouttickrate = 0
        p.camclipbehinddis = .4
        self.run_step(p, c, s, i)
        self.assertEqual((p.camclipbehinddis, p.camclipsidedis, p.camclipheightdis), (1, 1, 1))

    def test_direction_retains_original_temporal_order_and_signed_shoulders(self):
        p, c, s, i = self.setup()
        self.read(s, 50)
        a = self.run_step(p, c, s, i)
        self.assertEqual(a.direction.x, 0)  # previous camtrigangle/angleadjust = 0
        self.assertEqual(bits(a.direction.y), bits(-.2))
        angle = s.camtrigangle
        self.assertAlmostEqual(angle, math.atan(.3/1.3) * 2/math.pi, places=5)
        self.assertAlmostEqual(a.direction.z, -1 + angle, places=5)
        b = self.run_step(p, c, s, i)
        self.assertAlmostEqual(b.direction.x, -angle - 10/90, places=5)
        self.assertAlmostEqual(b.direction.z, -1 + angle + 10/90, places=5)
        p.sidedis = -1
        before_angle = s.camtrigangle
        d = self.run_step(p, c, s, i)
        self.assertAlmostEqual(d.direction.x, -before_angle + 10/90, places=5)
        self.assertLess(s.camtrigangle, 0)
        # -1 minus signed angle reflects at -1 BEFORE the degree adjustment.
        self.assertAlmostEqual(d.direction.z, -1 - s.camtrigangle - 10/90, places=5)

    def test_direction_sign_quadrants_and_one_reflection_only(self):
        p, c, s, i = self.setup()
        self.read(s, 50)
        c.camclipdegreeadjust = 0
        for x in (.6, -.6):
            for z in (.8, -.8):
                s.camtrigangle = .25
                s.camclipangleadjust = 0
                s.camisnorth = int(z >= 0)
                i.direction = Vec(x, -.3, z)
                o = self.run_step(p, c, s, i)
                self.assertAlmostEqual(o.direction.x, -x + (-.25 if z >= 0 else .25), places=6)
                self.assertAlmostEqual(o.direction.y, .3, places=6)
                expected = -z + (s.camtrigangle if x >= 0 else -s.camtrigangle)
                if expected >= 1: expected = 2 - expected
                if expected <= -1: expected = -2 - expected
                self.assertAlmostEqual(o.direction.z, expected, places=5)
        # Original compares negative zero as non-negative for west/north.
        i.direction = Vec(-0.0, 0, -0.0)
        self.run_step(p, c, s, i)
        self.assertEqual((s.camiswest, s.camisnorth), (1, 1))

    def test_zero_side_substitution_and_camera_distance_scaling(self):
        p, c, s, i = self.setup()
        self.read(s, .75)
        p.behinddis = 2
        p.sidedis = 0
        self.run_step(p, c, s, i)
        self.assertEqual(p.camclipbehinddis, .25)  # .75/(1.5*2)
        # Original intentionally allows this >1 minimum at end of a call;
        # only the next call's entry clamps before applying minima again.
        self.assertAlmostEqual(p.camclipsidedis, 70, places=4)
        self.assertGreater(s.camtrigangle, 0)

    def test_disabled_flags_reset_multipliers_but_do_not_clear_hysteresis(self):
        p, c, s, i = self.setup()
        for field, value in [('firstpersonflag', 1), ('tpcamclip', 0),
                             ('tpcamclip', 2), ('noclipautodisable', 1), ('stopdofflag', 0)]:
            i.flags = Flags(0, 1, 0, 1)
            setattr(i.flags, field, value)
            p.camclipbehinddis = p.camclipsidedis = p.camclipheightdis = .4
            s.oldwallping, s.bypassclipdeadzone = .6, 1
            before = bytes(s)
            o = self.run_step(p, c, s, i)
            self.assertEqual(o.status, 0)
            self.assertEqual(bytes(o.direction), bytes(i.direction))
            self.assertEqual((p.camclipbehinddis, p.camclipsidedis, p.camclipheightdis), (1, 1, 1))
            self.assertEqual(bytes(s), before)

    def test_distance_fp_delay_and_exact_exclusions(self):
        _, _, s, _ = self.setup()
        self.read(s, .8)
        self.read(s, .1, Flags(1, 1, 0, 1))
        self.assertEqual(s.rdofdelayframes, 3)
        self.assertEqual(bits(s.dofattempt), bits(.8))
        for expected in (2, 1, 0):
            self.assertEqual(self.read(s, .6), 0)
            self.assertEqual(s.rdofdelayframes, expected)
            self.assertEqual(s.dofattempt, 50)
        self.assertEqual(self.read(s, .6), 1)
        self.assertEqual(bits(s.dofattempt), bits(.6))
        self.assertEqual(self.read(s, .25), 0)
        self.assertEqual(s.rawdofattempt, .25)
        self.assertEqual(bits(s.dofattempt), bits(.6))
        self.assertEqual(self.read(s, .2, loadarea=37, playerx=8.5, playerz=116), 0)
        self.assertEqual(self.read(s, .2, loadarea=37, playerx=9.1, playerz=116), 1)
        self.assertEqual(self.read(s, .9, Flags(0, 0, 0, 1)), 1)
        self.assertEqual(s.viewdofattempt, 0)
        self.assertEqual(bits(s.dofattempt), bits(.9))

    def test_native_invalid_values_never_write_nonfinite_camera_or_switch_fp(self):
        p, c, s, i = self.setup()
        for value in (math.nan, math.inf, -1):
            self.assertEqual(self.read(s, value), 2)
            self.assertEqual(s.sample_valid, 0)
            o = self.run_step(p, c, s, i)
            self.assertEqual(o.status, 2)
            self.assertEqual(bytes(o.direction), bytes(i.direction))
            self.assertEqual((p.camclipbehinddis, p.camclipsidedis, p.camclipheightdis), (1, 1, 1))
            self.assertEqual(i.flags.firstpersonflag, 0)
        self.read(s, 1)
        for target, field, invalid in ((i, 'framerate', 0), (p, 'behinddis', 0),
                                       (c, 'camclipstartmult', math.inf)):
            previous = getattr(target, field)
            setattr(target, field, invalid)
            self.assertEqual(self.run_step(p, c, s, i).status, 2)
            setattr(target, field, previous)
        self.assertEqual(self.run_step(p, c, s, i).status, 1)


    def test_examine_distance_uses_secondary_sample_only_for_quarter_sentinel(self):
        _, _, s, _ = self.setup()
        self.assertIsNotNone(self.examine_write, 'original examine DOF capture is not implemented')
        flags = Flags(0, 1, 0, 1)
        self.read(s, .8)
        before = bytes(s)
        self.assertEqual(self.examine_write(C.byref(s), C.byref(flags), 1.2, 1), 0)
        self.assertEqual(bytes(s), before)
        self.read(s, .25)
        s.rdofdelayframes = 2
        self.assertEqual(self.examine_write(C.byref(s), C.byref(flags), 1.2, 1), 1)
        self.assertEqual(bits(s.dofattempt), bits(1.2))
        self.assertEqual(bits(s.viewdofattempt), bits(1.2))
        self.assertEqual(s.rawdofattempt, .25)
        self.assertEqual(s.rdofdelayframes, 2)
        for field, value in [('firstpersonflag', 1), ('tpcamclip', 0),
                             ('noclipautodisable', 1), ('stopdofflag', 0)]:
            flags = Flags(0, 1, 0, 1)
            setattr(flags, field, value)
            self.assertEqual(self.examine_write(C.byref(s), C.byref(flags), .4, 1), 1)
            self.assertEqual(bits(s.dofattempt), bits(.4))
            self.assertEqual(s.viewdofattempt, 0)
        flags = Flags(0, 2, 0, 1)  # source read checks zero, angle hook checks exactly 1
        self.examine_write(C.byref(s), C.byref(flags), .6, 1)
        self.assertEqual(bits(s.viewdofattempt), bits(.6))
        for value, available in ((math.nan, 1), (math.inf, 1), (-1, 1), (.5, 0)):
            self.assertEqual(self.examine_write(C.byref(s), C.byref(flags), value, available), 2)
            self.assertEqual(s.sample_valid, 0)
            self.assertEqual(bits(s.dofattempt), bits(.6))
        self.assertEqual(self.examine_write(C.byref(s), C.byref(flags), .7, 1), 1)
        self.assertEqual(s.sample_valid, 1)

    def test_private_source_full_collision_instruction_oracle(self):
        p, c, s, i = self.setup()
        path = Path(os.environ.get('RE7_CT_FILE', ROOT.parent/'original/re7.CT'))
        if not path.exists():
            self.skipTest('Private CT absent; set RE7_CT_FILE for original instruction comparison.')
        oracle = CollisionOracle(path, Path(self.tmp.name))
        rng = random.Random(8026443)
        for n in range(1800):
            # Realistic signed directions, both shoulders, unusual finite settings,
            # varying rate/FOV, and state continuity exercise original branches.
            p.pitchmul = rng.uniform(.3, 1)
            p.backdisadjust = rng.uniform(.8, 1.8)
            p.behinddis = rng.uniform(.5, 2)
            p.sidedisadjust = rng.uniform(.15, .5)
            p.sidedis = rng.choice((-1.5, -1, -.5, .5, 1, 1.5))
            p.camclipbehinddis = rng.uniform(.1, 1.2)
            p.camclipheightdis = rng.uniform(.1, 1.2)
            p.camclipsidedis = rng.uniform(.1, 1.2)
            c.camclipdegreeadjust = rng.uniform(-20, 20)
            c.clipzoomintickrate = rng.choice((0, .001, .2, 1))
            c.clipzoomouttickrate = rng.choice((0, .001, .2, 1))
            c.cczoomoutbuffer = rng.choice((0, .2, .6))
            c.camclipbackminmult = rng.uniform(0, .4)
            c.camclipsideminmult = rng.uniform(0, 1.1)
            i.direction = Vec(rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(-1, 1))
            i.yawx, i.yawz = rng.uniform(-1, 1), rng.uniform(-1, 1)
            i.camera_attempt = Vec(rng.uniform(-100, 100), 0, rng.uniform(-100, 100))
            i.autooldcamfov = rng.choice((25, 80))
            i.framerate = rng.choice((28, 30, 60, 120, 240))
            i.flags.firstpersonflag = int(n % 53 == 0)
            s.oldwallping = rng.uniform(0, 2)
            s.dofattempt = (s.oldwallping + .0002 if n % 13 == 0 else rng.uniform(.01, 5))
            s.bypassclipdeadzone = rng.randrange(2)
            values = {field: getattr(p, field) for field in PARAM_NAMES}
            values.update({field: getattr(c, field) for field in CONFIG_NAMES})
            values.update({field: getattr(s, field) for field in STATE_FLOATS})
            values.update({field: getattr(i.flags, field) for field, _ in Flags._fields_})
            values.update(camiswest=s.camiswest, camisnorth=s.camisnorth,
                          bypassclipdeadzone=s.bypassclipdeadzone,
                          sourcex=i.direction.x, sourcey=i.direction.y, sourcez=i.direction.z,
                          yawx=i.yawx, yawz=i.yawz, camxattempt=i.camera_attempt.x,
                          camzattempt=i.camera_attempt.z, framerate=i.framerate,
                          autooldcamfov=i.autooldcamfov)
            expected = oracle.run(values)
            o = self.run_step(p, c, s, i)
            self.assertNotEqual(o.status, 2, f'unexpected native validation at case {n}')
            for field in ('camclipbehinddis', 'camclipsidedis', 'camclipheightdis'):
                self.assertEqual(bits(getattr(p, field)), bits(expected[field]), (n, field))
            for field in ('camtrigangle', 'camclipangleadjust', 'oldwallping'):
                self.assertEqual(bits(getattr(s, field)), bits(expected[field]), (n, field))
            for field in ('camiswest', 'camisnorth', 'bypassclipdeadzone'):
                self.assertEqual(getattr(s, field), expected[field], (n, field))
            for axis in ('x', 'y', 'z'):
                self.assertEqual(bits(getattr(o.direction, axis)), bits(expected['output' + axis]), (n, axis))
            self.assertEqual(bits(o.direction.y), bits(expected['finalecx']))
            self.assertEqual(bits(o.direction.z), bits(expected['finaleax']))


if __name__ == '__main__':
    unittest.main()
