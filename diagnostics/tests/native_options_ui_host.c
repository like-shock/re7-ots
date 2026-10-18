#include <assert.h>
#include <stdio.h>
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "native_options.h"
#include "native_options_ui.h"
static NativeOptions options;
int native_options_ui_get(int*v,int*s){return native_options_get(&options,v,s);}
int native_options_ui_set(unsigned flag,int v){return native_options_request_field(&options,flag,v);}
static void run(lua_State*L,const char*s){int r=luaL_dostring(L,s);if(r)fprintf(stderr,"%s\n",lua_tostring(L,-1));assert(r==LUA_OK);}
int main(void){
 native_options_init(&options);native_options_load(&options,3);
 for(int pass=0;pass<2;pass++){
  lua_State*L=luaL_newstate();assert(L);luaL_openlibs(L);
  lua_pushboolean(L,pass==0);lua_setglobal(L,"expected_initial");
  run(L,"callbacks=0; re={on_draw_ui=function(f) callbacks=callbacks+1; draw=f end}; opened=false; clicks=0; text=''; shown={}; changes={}; imgui={tree_node=function(s) assert(s=='RE7 OTS Native'); return opened end, tree_pop=function() end, text=function(s) text=text..s end, checkbox=function(s,v) clicks=clicks+1; shown[s]=v; return changes[s]~=nil,changes[s] end}; melee='Auto-Disable with Melee Weapons'; head='Hide Head in First Person'");
  lua_pushinteger(L,42);
  assert(native_options_ui_register(L));assert(lua_gettop(L)==1);
  assert(native_options_ui_register(L));assert(lua_gettop(L)==1);
  run(L,"assert(callbacks==1); draw(); assert(clicks==0); opened=true; draw(); assert(shown[melee]==expected_initial); assert(shown[head]==expected_initial)");
  run(L,"changes[melee]=false; draw(); changes={}; draw(); assert(shown[melee]==false); assert(shown[head]==expected_initial)");
  int v;unsigned token;assert(native_options_pending(&options,&v,&token)&&v==(pass==0?2:0));
  native_options_complete(&options,token,0);
  run(L,"text=''; draw(); assert(text:find('Could not save',1,true))");
  /* Both clicks in one draw must survive, not overwrite each other. */
  run(L,"changes[melee]=true; changes[head]=true; draw()");
  assert(native_options_pending(&options,&v,&token)&&v==3);native_options_complete(&options,token,1);
  run(L,"changes[melee]=false; changes[head]=false; draw()");
  assert(native_options_pending(&options,&v,&token)&&v==0);native_options_complete(&options,token,1);
  lua_close(L);
 }
 lua_State*L=luaL_newstate();luaL_openlibs(L);assert(!native_options_ui_register(L));assert(lua_gettop(L)==0);
 run(L,"_VERSION='Lua 5.3'");assert(!native_options_ui_register(L));lua_close(L);
 return 0;
}
