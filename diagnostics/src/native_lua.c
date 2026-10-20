/* Static Lua linkage follows REFramework's official example plugin. */
#include <string.h>
#include "lua.h"
#include "native_lua.h"
_Static_assert(sizeof(lua_Integer)==8&&sizeof(lua_Number)==8,"REFramework Lua numeric ABI");
static int read_state(lua_State *L){
 char buffer[NATIVE_SNAPSHOT_CAP];
 size_t n=native_read_snapshot(buffer,sizeof buffer);
 if(n&&n<=sizeof buffer)lua_pushlstring(L,buffer,n);else lua_pushnil(L);
 return 1;
}
static int clock_seconds(lua_State *L){lua_pushnumber(L,native_clock());return 1;}
static int install(lua_State *L){
 lua_getglobal(L,"_VERSION");const char *version=lua_tostring(L,-1);
 int compatible=version&&strcmp(version,"Lua 5.4")==0;lua_pop(L,1);
 if(!compatible){lua_pushboolean(L,0);return 1;}
 lua_createtable(L,0,3);
 lua_pushinteger(L,1);lua_setfield(L,-2,"api_version");
 lua_pushcfunction(L,read_state);lua_setfield(L,-2,"read");
 lua_pushcfunction(L,clock_seconds);lua_setfield(L,-2,"clock");
 lua_setglobal(L,"re7_ots_native");lua_pushboolean(L,1);return 1;
}
int native_lua_register(void *state){
 lua_State *L=state;if(!L)return 0;
 int top=lua_gettop(L);lua_pushcfunction(L,install);
 int ok=lua_pcall(L,0,1,0)==LUA_OK&&lua_toboolean(L,-1);
 lua_settop(L,top);return ok;
}
