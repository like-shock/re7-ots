"""Private RT 8.02 -> executable x87 oracle; never stores the CT in git."""
import hashlib
import os
from pathlib import Path
import platform
import re
import subprocess
import tempfile
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
CT_HASH = '4154ee807842eb17f51b84272c98c3fdf153082c252f176446aa0cd61fd0686d'


@unittest.skipUnless(os.environ.get('RE7_CT_FILE'), 'private original CT not supplied')
class OriginalAudiovisualTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        raw = Path(os.environ['RE7_CT_FILE']).read_bytes()
        if hashlib.sha256(raw).hexdigest() != CT_HASH:
            raise ValueError('Original RT 8.02 hash mismatch')
        cls.text = raw.decode('utf-8-sig')

    def test_all_eight_rvas_and_spans_match_original_disassembly(self):
        from test_native_sites import site_rows
        instructions = {int(a, 16): bytes.fromhex(b) for a, b in
                        re.findall(r're7.exe\+([A-F0-9]+): ([0-9A-F ]+?)\s+- ', self.text)}
        for name, rva, size, expected in site_rows()[43:]:
            with self.subTest(name=name):
                actual = b''
                while len(actual) < size:
                    actual += instructions[rva+len(actual)]
                self.assertEqual(actual, expected)

    def test_autoactivated_default_overrides_flashlight_distance(self):
        root = ET.fromstring(self.text)
        master = root.find('CheatEntries/CheatEntry')
        self.assertEqual(master.find('Options').get('moActivateChildrenAsWell'), '1')
        entry = next(e for e in master.findall('CheatEntries/CheatEntry')
                     if e.findtext('ID') == '1337098455')
        self.assertRegex(entry.findtext('AssemblerScript'), r'flashbackdisadjust:\s+dd \(float\)0.2\b')

    def test_flashlight_coordinates_bit_match_extracted_x87(self):
        block = self.text.split('//Alternate Flashlight Coords (')[1].split('//Flashlight Color')[0]
        ranges = [block.split('//xpos')[1].split('//ypos')[0],
                  block.split('//ypos')[1].split('//zpos')[0],
                  block.split('//zpos')[1].split('{$except}')[0]]
        darwin = platform.system() == 'Darwin'
        assembly = ['.text']
        fields_by_axis = []
        for axis, text in enumerate(ranges):
            symbol = ('_' if darwin else '')+f'av_oracle_{axis}'
            assembly += [f'.globl {symbol}', symbol+':']
            fields = []
            def mem(name):
                if name not in fields: fields.append(name)
                return f'{4*fields.index(name)}(%rdi)'
            source = None
            for raw in text.splitlines():
                line = raw.split('//')[0].strip()
                m = re.fullmatch(r'mov r8,\[(yawx|yawz)\]', line)
                if m: source = m[1]
                elif line == 'mov [fstorage],r8d':
                    assembly += [f'movl {mem(source)},%eax', f'movl %eax,{mem("fstorage")}']
                elif line == 'mov [fstorage],(float)-1':
                    assembly += [f'movl $0xbf800000,{mem("fstorage")}']
                else:
                    m = re.fullmatch(r'(fld|fmul|fsub|fadd|fstp) \[(\w+)\]', line)
                    if m:
                        op, name = m.groups()
                        op = {'fld':'flds','fmul':'fmuls','fsub':'fsubs','fadd':'fadds','fstp':'fstps'}[op]
                        assembly.append(f'{op} {mem(name)}')
                    elif line.startswith(('fld', 'fmul', 'fadd', 'fsub', 'fstp')):
                        self.fail('Unhandled source instruction: '+line)
            assembly.append('ret')
            fields_by_axis.append(fields)
        if not darwin: assembly.append('.section .note.GNU-stack,"",@progbits')
        expressions = {'yawx':'value(0x1008)', 'yawz':'value(0x1000)',
                       'camxattempt':'r.camera_attempt.x', 'camyattempt':'r.camera_attempt.y',
                       'camzattempt':'r.camera_attempt.z', 'flashbackdisadjust':'.2f',
                       'tpflashheightadjust':'-.2f'}
        for field in ['backdisadjust','behinddis','aimbehinddis','pitchmul','sidedisadjust','sidedis','aimsidedis']:
            expressions[field] = 'r.params.'+field
        checks = []
        for axis, fields in enumerate(fields_by_axis):
            initial = ','.join(expressions.get(f, '0') for f in fields)
            checks.append(f'float v{axis}[]={{'+initial+f'}};av_oracle_{axis}(v{axis});'
                          f'CHECK(!memcmp(memory+0x4000+{axis*4},v{axis}+{fields.index("fstorage")},4));')
        c = '''#define main adapter_cases_main
#include "native_av_host.c"
#undef main
void av_oracle_0(float*),av_oracle_1(float*),av_oracle_2(float*);
int main(void){unsigned s=site("FLASHLIGHT_ALT_WRITE");
for(int j=0;j<2000;j++){
 setup();f.rsi=0x4000;number(0x3fc0,.9098039269f);
 number(0x1008,(j%201-100)*.0137f);number(0x1000,(j%193-96)*.0093f);
 r.camera_attempt=(Re7OriginalVec){(j-1000)*.731f,(j-900)*.127f,(j-800)*.311f};
 r.params.behinddis=.5f+(j%11)*.171f;r.params.aimbehinddis=.03f+(j%7)*.111f;
 r.params.pitchmul=1.3f-(j%13)*.17f;r.params.sidedis=(j&1)?1:-1;
 r.params.aimsidedis=.1f+(j%23)*.071f;
 CHECK(call(s));
'''+'\n'.join(checks)+'''\n}puts("2000 source-extracted flashlight XYZ cases passed");return 0;}'''
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp);(tmp/'oracle.S').write_text('\n'.join(assembly)+'\n');(tmp/'check.c').write_text(c)
            names = ['native_runtime','native_av','native_head','native_bridge','original_scalar',
                     'original_state','original_inputs','original_collision','original_interaction','original_model']
            bridge = ROOT/'src/native_bridge.S'
            if darwin:
                bridge = tmp/'link_only.c'
                bridge.write_text('\n'.join(f'void native_detour_{n}(void){{}}' for n in range(64)))
            command = ['cc', *(['-arch','x86_64'] if darwin else []), '-O2', '-Wall','-Wextra','-Werror',
                       '-ffp-contract=off','-fno-fast-math','-I'+str(ROOT/'src'),'-I'+str(ROOT/'tests'),
                       str(tmp/'check.c'),str(tmp/'oracle.S'),str(bridge),
                       *[str(ROOT/'src'/f'{n}.c') for n in names],'-lm','-o',str(tmp/'check')]
            result = subprocess.run(command,capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)
            result = subprocess.run([str(tmp/'check')],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)
