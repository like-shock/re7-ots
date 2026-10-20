import pathlib,struct,unittest
ROOT=pathlib.Path(__file__).resolve().parents[1]
class ProbeBinaryTests(unittest.TestCase):
 def test_windows_probe_abi_and_hook_only_imports(self):
  b=(ROOT/'build/RE7_OTS_NativeProbe.dll').read_bytes();u16=lambda p:struct.unpack_from('<H',b,p)[0];u32=lambda p:struct.unpack_from('<I',b,p)[0]
  nt=u32(60);self.assertEqual(b[nt:nt+4],b'PE\0\0');self.assertEqual(u16(nt+4),0x8664);self.assertTrue(u16(nt+22)&0x2000)
  op=nt+24;self.assertEqual(u16(op),0x20b);self.assertNotEqual(u32(op+16),0)
  for directory in [3,5]: self.assertNotEqual(u32(op+112+directory*8),0)
  sections=[]
  for i in range(u16(nt+6)):
   p=op+u16(nt+20)+40*i;sections.append((u32(p+12),u32(p+16),u32(p+20),u32(p+36)))
  self.assertFalse(any(flags&0x20000000 and flags&0x80000000 for _,_,_,flags in sections),'RWX section')
  def raw(r):
   for va,n,off,_ in sections:
    if va<=r<va+n:return off+r-va
   raise AssertionError(f'unmapped RVA {r:X}')
  def cstr(p):return b[p:b.index(0,p)].decode('ascii')
  p=raw(u32(op+120));deps=[];imports=[]
  while any(b[p:p+20]):
   deps.append(cstr(raw(u32(p+12))));q=raw(u32(p) or u32(p+16))
   while struct.unpack_from('<Q',b,q)[0]:
    t=struct.unpack_from('<Q',b,q)[0];self.assertFalse(t>>63);imports.append(cstr(raw(t)+2));q+=8
   p+=20
  self.assertEqual(set(deps),{'KERNEL32.dll','USER32.dll','msvcrt.dll'})
  self.assertFalse(set(imports)&{'WriteProcessMemory','VirtualProtectEx','CreateRemoteThread','LoadLibraryA','LoadLibraryW','WinHttpOpen','WSAStartup'})
  self.assertIn('ReadProcessMemory',imports);self.assertIn('Sleep',imports);self.assertIn('VirtualProtect',imports);self.assertIn('FlushInstructionCache',imports)
  p=raw(u32(op+112));q=raw(u32(p+32));names=[cstr(raw(u32(q+4*i))) for i in range(u32(p+24))]
  self.assertEqual(set(names),{'reframework_plugin_required_version','reframework_plugin_initialize'})
