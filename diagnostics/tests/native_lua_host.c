#include <assert.h>
#include <string.h>
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "native_lua.h"
static int fail;
size_t native_read_snapshot(char *out,size_t cap){assert(cap>=3);if(fail)return 0;memcpy(out,"{}",2);return 2;}
double native_clock(void){return 123.456;}
int main(void){
 for(int i=0;i<2;i++){
  lua_State *L=luaL_newstate();assert(L);luaL_openlibs(L);
  lua_pushinteger(L,42);assert(native_lua_register(L));assert(lua_gettop(L)==1);
  assert(luaL_dostring(L,"assert(re7_ots_native.api_version==1);assert(re7_ots_native.read()=='{}');assert(re7_ots_native.clock()==123.456)")==LUA_OK);
  fail=1;assert(luaL_dostring(L,"assert(re7_ots_native.read()==nil)")==LUA_OK);fail=0;
  assert(luaL_dostring(L,"_VERSION='Lua 5.3';re7_ots_native=nil")==LUA_OK);
  assert(!native_lua_register(L));assert(lua_gettop(L)==1);
  assert(luaL_dostring(L,"assert(re7_ots_native==nil)")==LUA_OK);
  assert(luaL_dostring(L,"_VERSION='Lua 5.4';setmetatable(_G,{__newindex=function() error('deny') end})")==LUA_OK);
  assert(!native_lua_register(L));assert(lua_gettop(L)==1);lua_close(L);
 }
 return 0;
}
