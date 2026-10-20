import importlib.util,json,pathlib,re,subprocess,tempfile,unittest
ROOT=pathlib.Path(__file__).resolve().parents[1]
class ProbeSiteTests(unittest.TestCase):
 def test_site_bytes_equal_original_manifest(self):
  rows=re.findall(r'\{"([A-Z0-9_]+)",0x([0-9A-F]+)u,(\d+),\{([^}]+)\}\}',(ROOT/'src/probe_sites.h').read_text())
  self.assertEqual(len(rows),5)
  manifest=json.loads((ROOT/'data/patch_manifest.json').read_text())
  for name,rva,n,values in rows:
   b=bytes(int(x,16) for x in values.split(','));self.assertEqual(len(b),int(n))
   self.assertEqual(b,bytes.fromhex(next(p['expected'] for p in manifest['patches'] if p['name']==name)))
   self.assertLess(int(rva,16)+int(n),161705984)
 def test_real_minhook_relocation_preserves_five_instruction_prefixes(self):
  with tempfile.TemporaryDirectory() as d:
   d=pathlib.Path(d)
   # Platform type declarations only; the original HDE/trampoline source runs.
   (d/'windows.h').write_text('''#pragma once
#include <stdint.h>
#include <string.h>
typedef int8_t INT8;typedef int16_t INT16;typedef int32_t INT32;typedef int64_t INT64;
typedef uint8_t UINT8;typedef uint16_t UINT16;typedef uint32_t UINT32;typedef uint64_t UINT64;
typedef unsigned UINT;typedef uintptr_t ULONG_PTR;typedef intptr_t LONG_PTR;
typedef void VOID;typedef void* LPVOID;typedef unsigned char* LPBYTE;
typedef uint32_t* PUINT32;typedef int BOOL;
#define TRUE 1
#define FALSE 0
''')
   (d/'check.c').write_text('''#include <windows.h>
#include "trampoline.h"
#include "probe_sites.h"
#include <assert.h>
BOOL IsExecutableAddress(LPVOID p){(void)p;return TRUE;}
int main(void){unsigned lengths[5]={5,6,8,5,5};
 for(unsigned i=0;i<5;i++){
  unsigned char target[64]={0},buf[64]={0};memcpy(target,probe_sites[i].expected,probe_sites[i].length);
  target[probe_sites[i].length]=0xC3;
  TRAMPOLINE ct={0};ct.pTarget=target;ct.pTrampoline=buf;ct.pDetour=(void*)0x12345678;
  assert(CreateTrampolineFunction(&ct));assert(!ct.patchAbove);assert(ct.nIP==2);
  assert(ct.oldIPs[1]==lengths[i]);assert(ct.newIPs[1]==lengths[i]);
  assert(!memcmp(buf,target,lengths[i]));assert(buf[lengths[i]]==0xFF&&buf[lengths[i]+1]==0x25);
  uintptr_t resume;memcpy(&resume,buf+lengths[i]+6,8);assert(resume==(uintptr_t)(target+lengths[i]));
 }return 0;}
''')
   mh=ROOT/'build/minhook/src'
   subprocess.run(['cc','-O2','-I'+str(d),'-I'+str(mh),'-I'+str(ROOT/'src'),str(d/'check.c'),str(mh/'trampoline.c'),str(mh/'hde/hde64.c'),'-o',str(d/'check')],check=True)
   subprocess.run([str(d/'check')],check=True)
 def test_pinned_dependency_detects_cache_tampering(self):
  spec=importlib.util.spec_from_file_location('fm',ROOT/'tools/fetch_minhook.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
  manifest=json.loads((ROOT/'data/minhook.json').read_text());m.verify(ROOT/'build/minhook',manifest)
  with tempfile.TemporaryDirectory() as d:
   d=pathlib.Path(d);(d/'x').write_bytes(b'changed')
   with self.assertRaises(ValueError):m.verify(d,{'files':{'x':'0'*64}})
