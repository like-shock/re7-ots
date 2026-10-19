"""Private RT8.02 FOV oracle: compile extracted instructions and resolve CE @f.

No original CT content is committed; only the translation and test fixtures.
"""
import hashlib
import os
from pathlib import Path
import platform
import re
import struct
import subprocess
import tempfile
import unittest
from test_native_fov import compile_host

CT_HASH = '4154ee807842eb17f51b84272c98c3fdf153082c252f176446aa0cd61fd0686d'


@unittest.skipUnless(os.environ.get('RE7_CT_FILE'), 'private original CT not supplied')
class OriginalFovTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        raw = Path(os.environ['RE7_CT_FILE']).read_bytes()
        if hashlib.sha256(raw).hexdigest() != CT_HASH:
            raise ValueError('Original RT8.02 hash mismatch')
        cls.text = raw.decode('utf-8-sig').replace('\r\n', '\n')

    def test_all_seven_fov_spans_are_complete_source_instructions(self):
        from test_native_sites import site_rows
        names = {'MENU_FOV_BYTE', 'TRUE_FOV_READ', 'REFRAMEWORK_FOV_WRITE_POINT',
                 'TRUE_FOV_REFERENCE', 'FOV_WRITE_MAIN', 'FOV_WRITE_NAH', 'FOV_WRITE_EOZ'}
        rows = [row for row in site_rows() if row[0] in names]
        self.assertEqual(len(rows), 7)
        instructions = {int(a, 16): bytes.fromhex(b) for a, b in
                        re.findall(r're7.exe\+([A-F0-9]+): ([0-9A-F ]+?)\s+- ', self.text)}
        for name, rva, size, expected in rows:
            with self.subTest(site=name):
                actual = b''
                while len(actual) < size:
                    actual += instructions[rva+len(actual)]
                self.assertEqual(actual, expected)

    def test_extracted_x87_branches_match_full_native_fov_trajectories(self):
        block = self.text.split('skipcamadjuststuff:')[1].split('norunfovadjust:')[0]
        lines = [line.split('//')[0].strip() for line in block.splitlines()]
        names = sorted(set(re.findall(r'\[(\w+)\]', block)) - {'rax'})
        names.append('reference_value')
        offsets = {name: index*4 for index, name in enumerate(names)}
        labels = [(index, line[:-1]) for index, line in enumerate(lines) if line.endswith(':')]
        anonymous = {index: 'anon_'+str(index) for index, name in labels if name == '@@'}
        darwin = platform.system() == 'Darwin'
        symbol = ('_' if darwin else '')+'fov_oracle'
        assembly = ['.intel_syntax noprefix', '.text', '.globl '+symbol, symbol+':']
        for index, line in enumerate(lines):
            if not line or line.startswith('{') or line in {'push rax', 'pop rax'}:
                continue
            if line.endswith(':'):
                assembly.append(anonymous.get(index, line[:-1])+':')
                continue
            if line == 'mov rax,[_truefovref]':
                continue
            if '@f' in line:
                target_index, target_name = next((i, n) for i, n in labels if i > index)
                line = line.replace('@f', anonymous.get(target_index, target_name))
            line = line.replace('[rax]', '[reference_value]')
            line = re.sub(r'\(float\)([-\d.]+)', lambda m: hex(struct.unpack('<I', struct.pack('<f', float(m[1])))[0]), line)
            line = re.sub(r'#(\d+)', lambda m: str(int(m[1])), line)
            line = line.replace('80000000', '0x80000000')
            line = re.sub(r'\[(\w+)\]', lambda m: '[rdi+'+str(offsets[m[1]])+']', line)
            if '[' in line and 'byte ptr' not in line:
                line = line.replace('[', 'DWORD PTR [')
            assembly.append(line)
        assembly += ['norunfovadjust:', 'code7:', 'ret']
        if not darwin:
            assembly.append('.section .note.GNU-stack,"",@progbits')
        defaults = {}
        for name in ('customfovflag', 'fov', 'preventactionfovchange', 'fprunfov',
                     'tprunfov', 'runningfovmax', 'fovrunincbaserate', 'fovrundecbaserate',
                     'reframeworkfovframes', 'zoomaimflag'):
            directive, value = re.findall(r'(?m)^'+name+r':\n\s+(db|dd) ([^\n]+)', self.text)[0]
            value = value.split('//')[0].strip()
            if value.startswith('(float)'):
                value = struct.unpack('<I', struct.pack('<f', float(value[7:])))[0]
            else:
                value = int(value.removeprefix('#'), 10 if value.startswith('#') else 16)
            defaults[name] = value
        initialization = '\n'.join(f'o[O_{name}]={value}u;' for name, value in defaults.items())
        c = '''#define main adapter_cases_main
#include "native_fov_host.c"
#undef main
void fov_oracle(uint32_t*);
enum {'''+','.join('O_'+name for name in names)+'''};
int main(void){
 const int32_t menus[]={0,1,4,8,20,-1,-14,-15,2147483647,-2147483647};
 const float rates[]={1,7,29.97f,59.94f,60,144,1000};
 unsigned cases=0;
 for(unsigned m=0;m<sizeof menus/sizeof menus[0];m++)for(unsigned fps=0;fps<7;fps++)
 for(unsigned mode=0;mode<8;mode++){
  setup();dword(MENU,(uint32_t)menus[m]);producers();
  uint32_t o['''+str(len(names))+''']={0};
  '''+initialization+'''
  o[O_menufovbyte]=(uint32_t)menus[m];o[O__truefovref]=1;o[O_reference_value]=bits(72);
  o[O_framerate]=bits(rates[fps]);r.inputs.state.framerate=rates[fps];
  r.fov.customfovflag=(mode&1)!=0;o[O_customfovflag]=r.fov.customfovflag;
  r.fov.preventactionfovchange=(mode&2)!=0;o[O_preventactionfovchange]=r.fov.preventactionfovchange;
  r.fov.tprunfov=(mode&4)==0;o[O_tprunfov]=r.fov.tprunfov;
  for(unsigned frame=0;frame<96;frame++){
   r.inputs.state.runflag=frame<32||frame>80;
   r.view.firstpersonflag=(frame>=40&&frame<72);
   r.state.stopflag=(frame>=74&&frame<78);
   r.inputs.state.flycamflag=(frame==79);
   o[O_runflag]=r.inputs.state.runflag;o[O_firstpersonflag]=r.view.firstpersonflag;
   o[O_stopflag]=r.state.stopflag;o[O_flycamflag]=r.inputs.state.flycamflag;
   fov_oracle(o);int handled=tick();
   if((o[O_actualfov]&0x7f800000u)!=0x7f800000u){
    if(!handled||get(TARGET)!=o[O_actualfov]){
     fprintf(stderr,"menu%d fps%g mode%u frame%u native%08x source%08x handled%d\\n",menus[m],rates[fps],mode,frame,get(TARGET),o[O_actualfov],handled);return 1;
    }
    CHECK(bits(r.fov.runningfovadd)==o[O_runningfovadd]);CHECK(r.fov.runfovcalcflag==o[O_runfovcalcflag]);
   }else CHECK(!handled);
   cases++;
  }
 }
 printf("%u source-extracted FOV frames passed\\n",cases);return 0;
}'''
        with tempfile.TemporaryDirectory() as directory:
            tmp = Path(directory)
            (tmp/'oracle.S').write_text('\n'.join(assembly)+'\n')
            (tmp/'check.c').write_text(c)
            exe = compile_host(tmp, tmp/'check.c', [tmp/'oracle.S'])
            result = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn('53760 source-extracted FOV frames passed', result.stdout)
