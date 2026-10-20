"""Private source disassembly and extracted x87/CMP oracles; no CT bundled."""
import hashlib, os, pathlib, platform, re, subprocess, tempfile, unittest
from test_native_fov import compile_host
ROOT=pathlib.Path(__file__).resolve().parents[1]
@unittest.skipUnless(os.environ.get('RE7_CT_FILE'),'private original CT not supplied')
class OptionalSourceTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  raw=pathlib.Path(os.environ['RE7_CT_FILE']).read_bytes()
  assert hashlib.sha256(raw).hexdigest()=='4154ee807842eb17f51b84272c98c3fdf153082c252f176446aa0cd61fd0686d'
  cls.text=raw.decode('utf-8-sig').replace('\r\n','\n')
 def test_all_11_optional_spans_are_exact_full_source_instructions(self):
  from test_native_sites import site_rows
  rows=site_rows()[67:];self.assertEqual(len(rows),11)
  instructions={int(a,16):bytes.fromhex(b) for a,b in re.findall(r're7.exe\+([A-F0-9]+): ([0-9A-F ]+?)\s+- ',self.text)}
  for name,rva,n,want in rows:
   with self.subTest(site=name):
    got=b''
    while len(got)<n:got+=instructions[rva+len(got)]
    self.assertEqual(got,want)
 def test_extracted_fullbright_color_rounding_and_shader_flags(self):
  darwin=platform.system()=='Darwin';prefix='_' if darwin else ''
  asm=['.intel_syntax noprefix','.text','.globl '+prefix+'color_oracle',prefix+'color_oracle:']
  block=self.text.split('aobscanmodule(LIGHT_SOURCE_DIAMETER_READ,')[1].split('//R Color')[1].split('//G Color')[0]
  ops=re.findall(r'^\s*(fild|fmul|fstp) \[(\w+)\]',block,re.M)
  self.assertEqual(len(ops),5)
  for op,name in ops:
   offset=4 if name=='fullbrightstor2' else 8 if name=='fbcolorbrightness' else 12 if op=='fmul' else 0
   asm.append(op+' DWORD PTR [rdi+'+str(offset)+']')
  asm+=['ret']
  for n in (17,18):
   # Locate unique shader AOB section before selecting its injected body.
   name='FLASHBACK_FILTER_READ' if n==17 else 'FLASHBACK_FILTER_READ_2'
   block=self.text.split('aobscanmodule('+name+',')[1].split(f'newmem{n}:')[1].split(f'{name}:')[0]
   lines=[x.split('//')[0].strip() for x in block.splitlines()]
   asm+=['.globl '+prefix+f'shader_oracle_{n}',prefix+f'shader_oracle_{n}:','mov rax,[rdi+32]','push QWORD PTR [rdi+40]','popfq']
   refs={'hurtfilterframes':0,'customsftoggle':4,'customscreenfilter':8,'attemptedfilter':12,'rdi+64':16,'rcx+28':16,'rcx+14':20,'rcx+38':20}
   for line in lines:
    if not line:continue
    if line.endswith(':'):line=f'oracle_{n}_'+line
    else:
     line=line.replace('push eax','push rax').replace('pop eax','pop rax').replace('#99','99')
     line=line.replace(f'code{n}',f'oracle_{n}_code{n}').replace(f'return{n}',f'oracle_{n}_return')
     line=re.sub(r'\[([^]]+)\]',lambda m:'[rdi+'+str(refs[m[1]])+']',line)
     if '[' in line and 'byte ptr' not in line:line=line.replace('[','DWORD PTR [')
    asm.append(line)
   asm+=[f'oracle_{n}_return:','mov [rdi+32],rax','pushfq','pop QWORD PTR [rdi+40]','cld','ret']
  if not darwin:asm+=['.section .note.GNU-stack,"",@progbits']
  unit=re.findall(r'fullbrightstor2:\n\s+dd \(float\)([\d.]+)',self.text)[0]
  c=r'''#define main adapter_main
#include "native_optional_host.c"
#undef main
void color_oracle(float*);void shader_oracle_17(void*);void shader_oracle_18(void*);
int main(void){
 for(unsigned j=0;j<512;j++){
  setup();uint32_t rgb=(j*65497u)&0xffffff;float brightness=(float)(j%21)*.5f;
  CHECK(native_runtime_set_fullbright(&r,1,rgb,brightness));light_fixture_observed();CHECK(call(NS_LIGHT_SOURCE_DIAMETER_READ));
  for(unsigned k=0;k<3;k++){
   float v[4];uint32_t component=(rgb>>(16-8*k))&255;memcpy(v,&component,4);v[1]=UNIT;v[2]=brightness;v[3]=150000;color_oracle(v);
   CHECK(!memcmp(fp+160+11*16+k*4,v,4));
  }
 }
 const uint32_t others[]={0,1,15,16,0x7fffffff,0x80000000,0xffffffff};
 for(unsigned n=0;n<2;n++)for(unsigned shader=0;shader<16;shader++)for(unsigned k=0;k<7;k++){
  setup();CHECK(native_runtime_set_custom_filter(&r,1,(int)shader));
  uint64_t o[6]={0};uint32_t words[]={0,1,shader,0,11,others[k]};memcpy(o,words,sizeof words);o[4]=f.rax;o[5]=f.flags;
  u32(f.rdi+0x64,11);u32(f.rcx+0x28,11);u32(f.rcx+0x14,others[k]);u32(f.rcx+0x38,others[k]);
  if(n)shader_oracle_18(o);else shader_oracle_17(o);
  CHECK(call(n?NS_FLASHBACK_FILTER_READ_2:NS_FLASHBACK_FILTER_READ));CHECK(f.rax==o[4]);CHECK((f.flags&0x8d5)==(o[5]&0x8d5));CHECK(r.screen.attemptedfilter==((uint32_t*)o)[3]);
 }
 puts("1536 extracted x87 color lanes and 224 source CMP cases: PASS");return 0;
}
'''.replace('UNIT',unit+'f')
  with tempfile.TemporaryDirectory() as directory:
   tmp=pathlib.Path(directory);(tmp/'oracle.S').write_text('\n'.join(asm)+'\n');(tmp/'check.c').write_text(c)
   exe=compile_host(tmp,tmp/'check.c',[tmp/'oracle.S'])
   p=subprocess.run([str(exe)],capture_output=True,text=True);self.assertEqual(p.returncode,0,p.stderr)
