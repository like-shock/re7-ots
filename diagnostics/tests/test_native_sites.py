"""Validate native hook metadata and execute the pinned MinHook decoder/relocator.

These checks validate source spans and relocation, not live game RVA identity.
The full device diagnostic containing the 42 RVAs is not a repository fixture.
"""
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def site_rows():
    rows = re.findall(r'\{"([A-Z0-9_]+)",0x([0-9A-F]+)u,(\d+),\{([^}]+)\}\}',
                      (ROOT/'src/native_sites.h').read_text())
    return [(name, int(rva, 16), int(n), bytes(int(x, 16) for x in values.split(',')))
            for name, rva, n, values in rows]


def verify_minhook():
    spec = importlib.util.spec_from_file_location('native_fetch_minhook', ROOT/'tools/fetch_minhook.py')
    fetch = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(fetch)
    manifest = json.loads((ROOT/'data/minhook.json').read_text())
    fetch.verify(ROOT/'build/minhook', manifest)
    return fetch


class NativeSiteTests(unittest.TestCase):
    def test_all_sites_have_complete_original_spans_and_distinct_metadata(self):
        rows = site_rows()
        self.assertEqual(len(rows), 42)
        self.assertEqual(len({name for name, _, _, _ in rows}), 42)
        self.assertEqual(len({rva for _, rva, _, _ in rows}), 42)
        enum = dict(re.findall(r'\bNS_([A-Z0-9_]+)=(\d+)',
                               (ROOT/'src/native_sites.h').read_text()))
        self.assertEqual(set(enum), {name for name, _, _, _ in rows})
        for index, (name, _, _, _) in enumerate(rows):
            self.assertEqual(int(enum[name]), index, 'dispatch enum/table mismatch: '+name)
        manifest = json.loads((ROOT/'data/patch_manifest.json').read_text())
        expected = {p['name']: bytes.fromhex(p['expected']) for p in manifest['patches']}
        for name, rva, length, data in rows:
            with self.subTest(name=name):
                self.assertEqual(length, len(data))
                self.assertEqual(data, expected[name])
                self.assertGreaterEqual(length, 5)
                self.assertLessEqual(length, 48)
                self.assertGreater(rva, 0)
                self.assertLess(rva + length, 161705984)
        ranges = sorted((rva, rva + length, name) for name, rva, length, _ in rows)
        for previous, current in zip(ranges, ranges[1:]):
            self.assertLessEqual(previous[1], current[0], (previous, current))
        # The five earlier probe RVAs are the only existing repository crosscheck.
        probe = re.findall(r'\{"([A-Z0-9_]+)",0x([0-9A-F]+)u',
                           (ROOT/'src/probe_sites.h').read_text())
        by_name = {name: rva for name, rva, _, _ in rows}
        for name, rva in probe:
            self.assertEqual(by_name[name], int(rva, 16))

    def test_real_minhook_relocates_all_42_prefixes_and_decodes_full_restore_spans(self):
        verify_minhook()
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            # Platform declarations only. Use the actual pinned HDE/MinHook code.
            (tmp/'windows.h').write_text('''#pragma once
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
            (tmp/'check.c').write_text('''#include <windows.h>
#include "trampoline.h"
#include "hde/hde64.h"
#include "native_sites.h"
#include <assert.h>
#include <stdio.h>
BOOL IsExecutableAddress(LPVOID p){(void)p;return TRUE;}
int main(void){
 assert(NATIVE_SITE_COUNT==42);
 for(unsigned i=0;i<NATIVE_SITE_COUNT;i++){
  unsigned char target[80]={0},buf[80]={0},boundaries[16]={0};
  memcpy(target,native_sites[i].expected,native_sites[i].length);
  target[native_sites[i].length]=0xC3;
  unsigned total=0,prefix=0,count=0;
  while(total<native_sites[i].length){
   hde64s instruction;unsigned n=hde64_disasm(target+total,&instruction);
   assert(n&&!(instruction.flags&F_ERROR));
   assert(total+n<=native_sites[i].length); /* complete restore span */
   assert(!(instruction.flags&F_RELATIVE));
   assert((instruction.modrm&0xC7)!=0x05); /* no RIP-relative fixture */
   if(total<5){boundaries[count++]=(unsigned char)total;prefix=total+n;}
   total+=n;
  }
  assert(total==native_sites[i].length&&prefix>=5);
  TRAMPOLINE ct={0};ct.pTarget=target;ct.pTrampoline=buf;ct.pDetour=(void*)0x12345678;
  assert(CreateTrampolineFunction(&ct));assert(!ct.patchAbove);assert(ct.nIP==count+1);
  for(unsigned j=0;j<count;j++){
   assert(ct.oldIPs[j]==boundaries[j]);assert(ct.newIPs[j]==boundaries[j]);
  }
  assert(ct.oldIPs[count]==prefix&&ct.newIPs[count]==prefix);
  assert(!memcmp(buf,target,prefix));
  assert(buf[prefix]==0xFF&&buf[prefix+1]==0x25);
  uintptr_t resume;memcpy(&resume,buf+prefix+6,8);
  assert(resume==(uintptr_t)(target+prefix));
  assert(ct.pRelay==buf+prefix+14);
  assert(((unsigned char*)ct.pRelay)[0]==0xFF&&((unsigned char*)ct.pRelay)[1]==0x25);
  uintptr_t detour;memcpy(&detour,(unsigned char*)ct.pRelay+6,8);
  assert(detour==(uintptr_t)ct.pDetour);
 }
 puts("native sites: 42 complete spans and real MinHook relocations PASS");return 0;
}
''')
            mh = ROOT/'build/minhook/src'
            subprocess.run(['cc', '-O2', '-Wall', '-Wextra', '-I'+str(tmp),
                            '-I'+str(mh), '-I'+str(ROOT/'src'), str(tmp/'check.c'),
                            str(mh/'trampoline.c'), str(mh/'hde/hde64.c'),
                            '-o', str(tmp/'check')], check=True)
            result = subprocess.run([str(tmp/'check')], check=True, capture_output=True, text=True)
            self.assertIn('42 complete spans and real MinHook relocations PASS', result.stdout)

    def test_pinned_dependency_rejects_tampered_cache(self):
        fetch = verify_minhook()
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            (tmp/'x').write_bytes(b'changed')
            with self.assertRaises(ValueError):
                fetch.verify(tmp, {'files': {'x': '0'*64}})
