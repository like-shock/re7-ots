import pathlib,struct,unittest
ROOT=pathlib.Path(__file__).resolve().parents[1]
class BinaryTests(unittest.TestCase):
 def test_pe_exports_imports_and_unwind_data(self):
  b=(ROOT/'build/RE7_OTS_Diagnostics.dll').read_bytes();u16=lambda p:struct.unpack_from('<H',b,p)[0];u32=lambda p:struct.unpack_from('<I',b,p)[0]
  nt=u32(60);self.assertEqual(b[nt:nt+4],b'PE\0\0');self.assertEqual(u16(nt+4),0x8664);self.assertTrue(u16(nt+22)&0x2000)
  op=nt+24;self.assertEqual(u16(op),0x20b);self.assertEqual(u32(op+16),0);self.assertNotEqual(u32(op+112+8*3),0);self.assertNotEqual(u32(op+112+8*5),0)
  sections=[]
  for i in range(u16(nt+6)):
   p=op+u16(nt+20)+40*i;sections.append((u32(p+12),u32(p+16),u32(p+20)))
  def raw(r):
   for va,n,off in sections:
    if va<=r<va+n:return off+r-va
   raise AssertionError(f'unmapped RVA {r:X}')
  def cstr(p):return b[p:b.index(0,p)].decode('ascii')
  p=raw(u32(op+112+8));deps=[];imports=[]
  while any(b[p:p+20]):
   deps.append(cstr(raw(u32(p+12))));q=raw(u32(p) or u32(p+16))
   while struct.unpack_from('<Q',b,q)[0]:
    t=struct.unpack_from('<Q',b,q)[0];self.assertFalse(t>>63);imports.append(cstr(raw(t)+2));q+=8
   p+=20
  self.assertEqual(set(deps),{'KERNEL32.dll','USER32.dll'})
  banned={'WriteProcessMemory','VirtualProtect','VirtualProtectEx','CreateRemoteThread','LoadLibraryA','LoadLibraryW','GetProcAddress','WinHttpOpen','WSAStartup'}
  self.assertFalse(banned.intersection(imports));self.assertIn('ReadProcessMemory',imports)
  p=raw(u32(op+112));n=u32(p+24);q=raw(u32(p+32));names=[cstr(raw(u32(q+4*i))) for i in range(n)]
  self.assertEqual(set(names),{'reframework_plugin_required_version','reframework_plugin_initialize'})
if __name__=='__main__':unittest.main()
