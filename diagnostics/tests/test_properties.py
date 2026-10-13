"""Differential tests against slow Python references, not a game-runtime test."""
import ctypes as C, hashlib, json, os, pathlib, random, subprocess, unittest
ROOT=pathlib.Path(__file__).resolve().parents[1]
class Pattern(C.Structure):_fields_=[('bytes',C.c_ubyte*256),('mask',C.c_ubyte*256),('n',C.c_uint),('anchor',C.c_uint),('anchor_n',C.c_uint),('shift',C.c_ushort*256)]
class Matches(C.Structure):_fields_=[('count',C.c_ulonglong),('rva',C.c_uint*8)]
class Sha(C.Structure):_fields_=[('h',C.c_uint*8),('total',C.c_ulonglong),('used',C.c_uint),('block',C.c_ubyte*64)]
class Properties(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  subprocess.run(['cc','-std=c11','-shared','-fPIC','-O2','-Isrc','src/core.c','-o','build/libcore.so'],cwd=ROOT,check=True)
  cls.lib=C.CDLL(str(ROOT/'build/libcore.so'));cls.lib.diag_pattern.argtypes=[C.POINTER(Pattern),C.c_char_p];cls.lib.diag_pattern.restype=C.c_int
  cls.lib.diag_scan.argtypes=[C.c_void_p,C.c_size_t,C.c_uint,C.c_size_t,C.POINTER(Pattern),C.POINTER(Matches)]
  cls.lib.diag_sha_init.argtypes=[C.POINTER(Sha)];cls.lib.diag_sha_update.argtypes=[C.POINTER(Sha),C.c_void_p,C.c_size_t];cls.lib.diag_sha_final.argtypes=[C.POINTER(Sha),C.c_void_p]
 def scan(self,b,p,start=0,old=0,m=None):
  if m is None:m=Matches()
  self.lib.diag_scan(C.create_string_buffer(b),len(b),start,old,C.byref(p),C.byref(m));return m
 def test_differential_masked_patterns_500_cases(self):
  rng=random.Random(8144)
  for _ in range(500):
   tokens=[rng.choice([f'{rng.randrange(256):02X}', '??','A?','?B']) for _ in range(rng.randrange(1,65))];tokens[rng.randrange(len(tokens))]='AA'
   p=Pattern();self.assertTrue(self.lib.diag_pattern(C.byref(p),' '.join(tokens).encode()))
   b=bytearray(rng.randbytes(512));at=rng.randrange(512-p.n+1)
   for k in range(p.n):b[at+k]=(b[at+k]&(~p.mask[k]&255))|p.bytes[k]
   expected=[i for i in range(len(b)-p.n+1) if all((b[i+k]&p.mask[k])==p.bytes[k] for k in range(p.n))]
   m=self.scan(bytes(b),p);self.assertEqual(m.count,len(expected));self.assertEqual(list(m.rva)[:min(8,m.count)],expected[:8])
 def test_stream_boundaries_all_manifest_patterns(self):
  manifest=json.loads((ROOT/'data/patch_manifest.json').read_text())
  for d in manifest['patches']:
   p=Pattern();self.assertTrue(self.lib.diag_pattern(C.byref(p),d['pattern'].encode()))
   b=bytearray(b'\xCC'*2000)
   for at in (100,400,900,1400):
    for k in range(p.n):b[at+k]=(b[at+k]&(~p.mask[k]&255))|p.bytes[k]
   direct=self.scan(bytes(b),p);stream=Matches();tail=b''
   for pos in range(0,len(b),73):
    chunk=bytes(b[pos:pos+73]);block=tail+chunk;self.scan(block,p,pos-len(tail),len(tail),stream);tail=block[-255:]
   self.assertEqual(stream.count,direct.count,d['name']);self.assertEqual(list(stream.rva),list(direct.rva),d['name'])
 def test_sha256_against_hashlib(self):
  rng=random.Random(7)
  for n in [0,1,55,56,63,64,65,127,128,1024,65536,1000000]:
   data=rng.randbytes(n);s=Sha();self.lib.diag_sha_init(C.byref(s))
   for pos in range(0,n,113):b=data[pos:pos+113];self.lib.diag_sha_update(C.byref(s),C.create_string_buffer(b),len(b))
   out=(C.c_ubyte*32)();self.lib.diag_sha_final(C.byref(s),out);self.assertEqual(bytes(out),hashlib.sha256(data).digest())
if __name__=='__main__':unittest.main()
