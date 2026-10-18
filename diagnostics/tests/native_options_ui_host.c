#include <assert.h>
#include <stdio.h>
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "native_options.h"
#include "native_options_ui.h"
static NativeOptions options;
int native_options_ui_get(int*v,int*s){return native_options_get(&options,v,s);}
int native_options_ui_set(int v){return native_options_request(&options,v);}
static void run(lua_State*L,const char*s){int r=luaL_dostring(L,s);if(r)fprintf(stderr,"%s\n",lua_tostring(L,-1));assert(r==LUA_OK);}
int main(void){
 native_options_init(&options);native_options_load(&options,1);
 for(int pass=0;pass<2;pass++){
  lua_State*L=luaL_newstate();assert(L);luaL_openlibs(L);
  lua_pushboolean(L,pass==0);lua_setglobal(L,"expected_initial");
  run(L,"callbacks=0; re={on_draw_ui=function(f) callbacks=callbacks+1; draw=f end}; opened=false; clicks=0; text=''; imgui={tree_node=function(s) assert(s=='RE7 OTS Native'); return opened end, tree_pop=function() end, text=function(s) text=text..s end, checkbox=function(s,v) assert(s=='Auto-Disable with Melee Weapons'); clicks=clicks+1; shown=v; return change,next_value end}");
  lua_pushinteger(L,42);
  assert(native_options_ui_register(L));assert(lua_gettop(L)==1);
  assert(native_options_ui_register(L));assert(lua_gettop(L)==1);
  run(L,"assert(callbacks==1); draw(); assert(clicks==0); opened=true; change=false; draw(); assert(shown==expected_initial)");
  run(L,"change=true; next_value=false; draw(); change=false; draw(); assert(shown==false)");
  int v;unsigned token;assert(native_options_pending(&options,&v,&token)&&v==0);
  native_options_complete(&options,token,0);
  run(L,"text=''; draw(); assert(text:find('Could not save',1,true))");
  run(L,"change=true; next_value=true; draw()");assert(native_options_pending(&options,&v,&token)&&v==1);native_options_complete(&options,token,1);
  run(L,"next_value=false; draw()");assert(native_options_pending(&options,&v,&token)&&v==0);native_options_complete(&options,token,1);
  lua_close(L); /* Lua reset must not reset native setting/persistence state. */
 }
 lua_State*L=luaL_newstate();luaL_openlibs(L);assert(!native_options_ui_register(L));assert(lua_gettop(L)==0);
 run(L,"_VERSION='Lua 5.3'");assert(!native_options_ui_register(L));lua_close(L);
 return 0;
}
