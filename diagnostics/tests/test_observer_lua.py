import ctypes as C,ctypes.util,pathlib,unittest
ROOT=pathlib.Path(__file__).resolve().parents[1]
class ObserverLuaTests(unittest.TestCase):
 def run_harness(self,tail=None,before=""):
  path=ROOT/'lua/re7_ots_observer.lua'
  self.assertTrue(path.exists(),'camera observer adapter is not implemented')
  lib=ctypes.util.find_library('lua5.4')
  if not lib: self.fail('liblua5.4 required for observer release tests')
  lua=C.CDLL(lib);lua.luaL_newstate.restype=C.c_void_p;lua.luaL_openlibs.argtypes=[C.c_void_p];lua.luaL_loadstring.argtypes=[C.c_void_p,C.c_char_p];lua.lua_pcallk.argtypes=[C.c_void_p,C.c_int,C.c_int,C.c_int,C.c_int64,C.c_void_p];lua.lua_tolstring.argtypes=[C.c_void_p,C.c_int,C.c_void_p];lua.lua_tolstring.restype=C.c_char_p;lua.lua_close.argtypes=[C.c_void_p]
  L=lua.luaL_newstate();lua.luaL_openlibs(L)
  try:
   source=(ROOT/'tests/observer_lua_harness.lua').read_text()
   if tail is not None: source=source.split('-- CASES BEGIN')[0]+tail
   code=source.replace('-- INSERT ADAPTER HERE',before+'\n'+path.read_text()).encode()
   rc=lua.luaL_loadstring(L,code)
   if not rc:rc=lua.lua_pcallk(L,0,0,0,0,None)
   msg=lua.lua_tolstring(L,-1,None) if rc else b''
   self.assertEqual(rc,0,msg.decode())
  finally:lua.lua_close(L)

 def test_observer_sdk_adapter_lifecycle(self): self.run_harness()
 def test_camera_transform_is_resolved_through_owner(self):
  self.run_harness("""status(1);step();status(2);step()
assert(last_sample().camera.status=='READABLE_CANDIDATE','camera must resolve Transform through its owning GameObject')
assert(owner_calls==1 and transform_calls==1,'both getters must be used for every sample')
assert(last_sample().camera.xyz[3]==3 and last_sample().camera.rotation_xyzw[4]==1)
status(3);step();assert(owner_calls==2 and transform_calls==2,'never cache the owner or transform')
assert(records[1].version=='0.2.1' and records[1].native_version=='0.2.0','Lua patch must accept the installed native DLL version')
""")
 def test_missing_owner_method_does_not_attempt_getter(self):
  self.run_harness("""missing_owner_method=true;status(1);step();status(2);step()
assert(last_sample().camera.status=='UNKNOWN_GAMEOBJECT_METHOD')
assert(owner_calls==0 and transform_calls==0 and last_sample().camera.xyz==nil)
missing_owner_method=false;status(3);step();assert(last_sample().camera.status=='READABLE_CANDIDATE')
""")
 def test_null_owner_invalidates_coordinates_and_recovers(self):
  self.run_harness("""status(1);step();status(2);step()
nil_owner=true;status(3);step()
assert(last_sample().camera.status=='UNKNOWN_NO_GAMEOBJECT')
assert(last_sample().camera.xyz==nil and transform_calls==1,'null owner must omit stale coordinates')
nil_owner=false;transform_id=21;status(4);step()
assert(last_sample().camera.status=='READABLE_CANDIDATE' and last_sample().camera.generation==2)
""")
 def test_stale_native_cannot_bypass_camera_deadline(self):
  self.run_harness("""status(1);step();status(2);step();tick=tick+901;frame();ui()
assert(records[#records].kind=='end' and records[#records].reason=='LIMIT','stale deadline must close camera file')
assert(not table.concat(texts,'|'):find('File: RECORDING',1,true))
""")
 def test_malformed_status_cannot_bypass_camera_deadline(self):
  self.run_harness("""status(1);step();status(2);step();tick=tick+901;fail_json=true;frame()
assert(records[#records].kind=='end' and records[#records].reason=='LIMIT','malformed status deadline must close camera file')
""")
 def test_missing_callback_diagnostic_survives_poll(self):
  self.run_harness("""status(1);tick=1;frame();status(2);tick=2;frame();ui()
assert(table.concat(texts,'|'):find('UNKNOWN_CALLBACK_API',1,true),'missing callback cause must stay visible')
""",'re.on_application_entry=nil')
 def test_callback_registration_exception_survives_poll(self):
  self.run_harness("""status(1);tick=1;frame();status(2);tick=2;frame();ui()
assert(table.concat(texts,'|'):find('UNKNOWN_CALLBACK_REGISTRATION',1,true),'callback registration cause must stay visible')
""",'re.on_application_entry=function() error("unsupported") end')
