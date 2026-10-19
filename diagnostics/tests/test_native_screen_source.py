"""Private RT 8.02 spans and extracted psycho arithmetic. CT stays outside git."""
import hashlib
import os
from pathlib import Path
import platform
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
CT_HASH = '4154ee807842eb17f51b84272c98c3fdf153082c252f176446aa0cd61fd0686d'


@unittest.skipUnless(os.environ.get('RE7_CT_FILE'), 'private original CT not supplied')
class OriginalScreenTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        raw = Path(os.environ['RE7_CT_FILE']).read_bytes()
        if hashlib.sha256(raw).hexdigest() != CT_HASH:
            raise ValueError('Original RT 8.02 hash mismatch')
        cls.text = raw.decode('utf-8-sig').replace('\r\n', '\n')

    def test_five_complete_spans_match_source_disassembly(self):
        from test_native_sites import site_rows
        instructions = {int(a, 16): bytes.fromhex(b) for a, b in
                        re.findall(r're7.exe\+([A-F0-9]+): ([0-9A-F ]+?)\s+- ', self.text)}
        rows = [row for row in site_rows() if row[0] in {
            'VIGNETTE', 'PSYCHO_TIMER', 'CLOSE_DISTORTION',
            'NAH_DOT_FORCE', 'NAH_CRIT_HP_EFFECT_CMP'}]
        self.assertEqual(len(rows), 5)
        for name, rva, size, expected in rows:
            with self.subTest(site=name):
                actual = b''
                while len(actual) < size:
                    actual += instructions[rva+len(actual)]
                self.assertEqual(actual, expected)

    def test_psycho_bits_match_extracted_x87_with_source_defaults(self):
        block = self.text.split('psychocheck:')[1].split('code10:')[0]
        branches = block.split('@@:')
        self.assertEqual(len(branches), 2)
        darwin = platform.system() == 'Darwin'
        assembly = ['.text']
        defaults = []
        for index, branch in enumerate(branches):
            symbol = ('_' if darwin else '')+f'screen_oracle_{index}'
            assembly += [f'.globl {symbol}', symbol+':']
            instructions = re.findall(r'(?m)^\s*(fld|fmul|fstp) \[(\w+)\]', branch)
            self.assertEqual(len(instructions), 3)
            for op, operand in instructions:
                if operand == 'fstorage':
                    address = '0(%rdi)'
                else:
                    value = re.findall(r'(?m)^'+operand+r':\n\s+dd \(float\)([^\n]+)', self.text)[-1]
                    defaults.append(value.strip())
                    address = '4(%rdi)'
                assembly.append({'fld':'flds', 'fmul':'fmuls', 'fstp':'fstps'}[op]+' '+address)
            assembly.append('ret')
        if not darwin:
            assembly.append('.section .note.GNU-stack,"",@progbits')
        c = '''#define main adapter_cases_main
#include "native_screen_host.c"
#undef main
void screen_oracle_0(float*),screen_oracle_1(float*);
int main(void){
 unsigned p=site("PSYCHO_TIMER"),s=site("CLOSE_DISTORTION");
 const float multipliers[]={'''+','.join(defaults)+'''};
 /* Finite signed normals/subnormals, infinities and quiet NaNs. The source
  * performs masked x87, and MOVSS reloads clear all three upper lanes. */
 const uint32_t inputs[]={0,0x80000000,1,0x80000001,0x007fffff,0x807fffff,
  0x00800000,0x80800000,0x3dcccccd,0xbd75c28f,0x3f800000,0xbf800000,
  0x7f7fffff,0xff7fffff,0x7f800000,0xff800000,0x7fc12345,0xffc12345};
 for(unsigned j=0;j<2000;j++)for(unsigned first=0;first<2;first++){
  setup();r.view.firstpersonflag=(uint8_t)first;f.rbx=M+0x2000;dword(f.rbx+0x240,0);CHECK(call(p));
  float v[2];uint32_t input=j<sizeof(inputs)/sizeof(inputs[0])?inputs[j]:(0x3a000000u+j*7919u);
  memcpy(v,&input,4);v[1]=multipliers[first];
  memcpy(fp+176,&input,4);f.rcx=M+0x3000;qword(f.rcx+0x18,123);
  if(first)screen_oracle_1(v);else screen_oracle_0(v);
  CHECK(call(s));CHECK(!memcmp(fp+176,v,4)&&!memcmp(fp+192,v,4));
  const uint32_t zero[3]={0};CHECK(!memcmp(fp+180,zero,12)&&!memcmp(fp+196,zero,12));
 }
 puts("4000 source-extracted psycho cases passed");return 0;
}'''
        with tempfile.TemporaryDirectory() as directory:
            tmp = Path(directory)
            (tmp/'oracle.S').write_text('\n'.join(assembly)+'\n')
            (tmp/'check.c').write_text(c)
            bridge = ROOT/'src/native_bridge.S'
            if darwin:
                bridge = tmp/'unexecuted_detours.c'
                bridge.write_text('\n'.join(f'void native_detour_{n}(void){{}}' for n in range(64)))
            names = ['native_runtime', 'native_screen', 'native_av', 'native_head', 'native_bridge',
                     'original_scalar', 'original_state', 'original_inputs',
                     'original_collision', 'original_interaction', 'original_model']
            result = subprocess.run(['cc', *(['-arch','x86_64'] if darwin else []), '-std=c11',
                                     '-O2', '-Wall','-Wextra','-Werror', '-ffp-contract=off',
                                     '-fno-fast-math','-I'+str(ROOT/'src'),'-I'+str(ROOT/'tests'),
                                     str(tmp/'check.c'),str(tmp/'oracle.S'),str(bridge),
                                     *[str(ROOT/'src'/f'{name}.c') for name in names],
                                     '-lm','-o',str(tmp/'check')],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)
            result = subprocess.run([str(tmp/'check')],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)
            self.assertIn('4000 source-extracted psycho cases passed',result.stdout)
