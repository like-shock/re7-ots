/* Same pinned Lua-core linkage as the existing REFramework native bridge.
 * Embedded menu only: no on_frame callback, SDK traversal or camera writes. */
#include <string.h>
#include "lua.h"
#include "native_options_ui.h"
_Static_assert(sizeof(lua_Integer)==8&&sizeof(lua_Number)==8,"REFramework Lua numeric ABI");
static const char menu[]=
 "local api=...\n"
 "assert(type(re)=='table' and type(re.on_draw_ui)=='function')\n"
 "assert(type(imgui)=='table' and type(imgui.tree_node)=='function' and type(imgui.checkbox)=='function' and type(imgui.text)=='function' and type(imgui.tree_pop)=='function')\n"
 "re.on_draw_ui(function()\n"
 " if imgui.tree_node('RE7 OTS Native') then\n"
 "  local ready,value,status=api.get()\n"
 "  if ready then\n"
 "   local changed,new_value=imgui.checkbox('Auto-Disable with Melee Weapons',value)\n"
 "   if changed then api.set(new_value) end\n"
 "   imgui.text('OFF keeps third person for melee weapons. Other automatic transitions still apply.')\n"
 "   if status==-1 then imgui.text('Could not save. This change applies to this session only.')\n"
 "   elseif status==0 then imgui.text('Saving setting...') end\n"
 "  else imgui.text('Native settings are not ready.') end\n"
 "  imgui.tree_pop()\n"
 " end\n"
 "end)\n";
static int get(lua_State*L){
 int value=1,status=0,ready=native_options_ui_get(&value,&status);
 lua_pushboolean(L,ready);lua_pushboolean(L,value);lua_pushinteger(L,status);return 3;
}
static int set(lua_State*L){
 int ok=lua_type(L,1)==LUA_TBOOLEAN&&native_options_ui_set(lua_toboolean(L,1));
 lua_pushboolean(L,ok);return 1;
}
static const char*reader(lua_State*L,void*ctx,size_t*size){
 (void)L;int*read=ctx;if((*read)++){*size=0;return NULL;}*size=sizeof menu-1;return menu;
}
static int install(lua_State*L){
 lua_getglobal(L,"_VERSION");const char*v=lua_tostring(L,-1);
 int compatible=v&&strcmp(v,"Lua 5.4")==0;lua_pop(L,1);
 if(!compatible){lua_pushboolean(L,0);return 1;}
 lua_getfield(L,LUA_REGISTRYINDEX,"RE7_OTS_Native.options_ui");int exists=lua_toboolean(L,-1);lua_pop(L,1);
 if(exists){lua_pushboolean(L,1);return 1;}
 int read=0;if(lua_load(L,reader,&read,"RE7_OTS_Native/options_ui","t")!=LUA_OK)return lua_error(L);
 lua_createtable(L,0,2);lua_pushcfunction(L,get);lua_setfield(L,-2,"get");lua_pushcfunction(L,set);lua_setfield(L,-2,"set");
 lua_call(L,1,0);
 lua_pushboolean(L,1);lua_setfield(L,LUA_REGISTRYINDEX,"RE7_OTS_Native.options_ui");
 lua_pushboolean(L,1);return 1;
}
int native_options_ui_register(void*state){
 if(!state)return 0;
 lua_State*L=state;
 int top=lua_gettop(L);lua_pushcfunction(L,install);
 int ok=lua_pcall(L,0,1,0)==LUA_OK&&lua_toboolean(L,-1);lua_settop(L,top);return ok;
}
