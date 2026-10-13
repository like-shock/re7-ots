import ctypes as C
import ctypes.util

def run_lua(code):
 lib=ctypes.util.find_library('lua5.4')
 assert lib, 'liblua5.4 is required'
 lua=C.CDLL(lib)
 lua.luaL_newstate.restype=C.c_void_p
 lua.luaL_openlibs.argtypes=[C.c_void_p]
 lua.luaL_loadstring.argtypes=[C.c_void_p,C.c_char_p]
 lua.lua_pcallk.argtypes=[C.c_void_p,C.c_int,C.c_int,C.c_int,C.c_int64,C.c_void_p]
 lua.lua_tolstring.argtypes=[C.c_void_p,C.c_int,C.c_void_p]
 lua.lua_tolstring.restype=C.c_char_p
 lua.lua_close.argtypes=[C.c_void_p]
 state=lua.luaL_newstate();lua.luaL_openlibs(state)
 try:
  rc=lua.luaL_loadstring(state,code.encode())
  if not rc:rc=lua.lua_pcallk(state,0,0,0,0,None)
  if rc:raise AssertionError(lua.lua_tolstring(state,-1,None).decode())
 finally:lua.lua_close(state)
