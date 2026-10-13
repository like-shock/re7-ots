import ctypes as C, ctypes.util, pathlib, unittest
ROOT=pathlib.Path(__file__).resolve().parents[1]
class LuaTests(unittest.TestCase):
 def test_status_ui_with_stubbed_reframework_bindings(self):
  lib=ctypes.util.find_library('lua5.4')
  if not lib:self.skipTest('liblua5.4 not installed')
  lua=C.CDLL(lib);lua.luaL_newstate.restype=C.c_void_p;lua.luaL_openlibs.argtypes=[C.c_void_p];lua.luaL_loadstring.argtypes=[C.c_void_p,C.c_char_p];lua.lua_pcallk.argtypes=[C.c_void_p,C.c_int,C.c_int,C.c_int,C.c_ssize_t,C.c_void_p];lua.lua_tolstring.argtypes=[C.c_void_p,C.c_int,C.c_void_p];lua.lua_tolstring.restype=C.c_char_p;lua.lua_close.argtypes=[C.c_void_p]
  L=lua.luaL_newstate();self.assertTrue(L);lua.luaL_openlibs(L)
  pre='''local ui; local tick=1; local fixture=nil; local fail=false; local texts={}; local pops=0
os.time=function() return tick end
re={on_draw_ui=function(f) ui=f end}
json={load_file=function(path) assert(path=="RE7_OTS_Diagnostics/status.json");if fail then error("partial JSON") end;return fixture end}
imgui={tree_node=function() return true end, text=function(s) texts[#texts+1]=s end,button=function() return false end, tree_pop=function() pops=pops+1 end}
'''
  post='''
assert(type(ui)=="function");ui();assert(table.concat(texts,"|"):find("No status yet",1,true))
fixture={phase="COMPLETE",report="re7_ots_diagnostic_example.txt"};tick=2;texts={};ui();assert(table.concat(texts,"|"):find("Scan finished",1,true))
fixture={phase="ERROR"};tick=3;texts={};ui();assert(table.concat(texts,"|"):find("Diagnostic error",1,true))
fail=true;tick=4;texts={};ui();assert(pops==4);assert(table.concat(texts,"|"):find("earlier launch",1,true))
'''
  try:
   code=(pre+(ROOT/'lua/re7_ots_diagnostic_status.lua').read_text()+post).encode();rc=lua.luaL_loadstring(L,code)
   if not rc:rc=lua.lua_pcallk(L,0,0,0,0,None)
   err=lua.lua_tolstring(L,-1,None) if rc else None;self.assertEqual(rc,0,err.decode() if err else '')
  finally:lua.lua_close(L)
if __name__=='__main__':unittest.main()
