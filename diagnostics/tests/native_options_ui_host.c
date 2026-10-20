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
int native_options_ui_get_settings(NativeSettings*s,int*status){return native_settings_get(&options,s,status);}
int native_options_ui_set_setting(unsigned id,double value){return native_settings_request(&options,id,value);}
static void run(lua_State*L,const char*s){int r=luaL_dostring(L,s);if(r)fprintf(stderr,"%s\n",lua_tostring(L,-1));assert(r==LUA_OK);}
int main(void){
 native_options_init(&options);native_options_load(&options,7);
 for(int pass=0;pass<2;pass++){
  lua_State*L=luaL_newstate();assert(L);luaL_openlibs(L);
  lua_pushboolean(L,pass==0);lua_setglobal(L,"expected_initial");
  run(L,"callbacks=0; re={on_draw_ui=function(f) callbacks=callbacks+1; draw=f end}; opened=false; clicks=0; text=''; shown={}; changes={}; imgui={tree_node=function(s) assert(s=='RE7 OTS Native'); return opened end, tree_pop=function() end, text=function(s) text=text..s end, checkbox=function(s,v) clicks=clicks+1; shown[s]=v; return changes[s]~=nil,changes[s] end}; melee='Auto-Disable with Melee Weapons'; head='Hide Head in First Person'; contrast='Limit Light Source Contrast in Third Person'; imgui.combo=function(s,v,values) shown[s]=v; assert(values[v]); return changes[s]~=nil,changes[s] end; imgui.slider_int=function(s,v,lo,hi) assert(lo==0 and hi==255); shown[s]=v; return changes[s]~=nil,changes[s] end; imgui.slider_float=function(s,v,lo,hi) assert(lo==0 and hi==10); shown[s]=v; return changes[s]~=nil,changes[s] end");
  lua_pushinteger(L,42);
  assert(native_options_ui_register(L));assert(lua_gettop(L)==1);
  assert(native_options_ui_register(L));assert(lua_gettop(L)==1);
  run(L,"assert(callbacks==1); draw(); assert(clicks==0); opened=true; draw(); assert(shown[melee]==expected_initial); assert(shown[head]==expected_initial); assert(shown[contrast]==expected_initial)");
  run(L,"changes[melee]=false; draw(); changes={}; draw(); assert(shown[melee]==false); assert(shown[head]==expected_initial); assert(shown[contrast]==expected_initial)");
  int v;unsigned token;assert(native_options_pending(&options,&v,&token)&&v==(pass==0?6:0));
  native_options_complete(&options,token,0);
  run(L,"text=''; draw(); assert(text:find('Could not save',1,true))");
  /* Both clicks in one draw must survive, not overwrite each other. */
  run(L,"changes[melee]=true; changes[head]=true; changes[contrast]=true; draw()");
  assert(native_options_pending(&options,&v,&token)&&v==7);native_options_complete(&options,token,1);
  run(L,"changes[melee]=false; changes[head]=false; changes[contrast]=false; draw()");
  assert(native_options_pending(&options,&v,&token)&&v==0);native_options_complete(&options,token,1);
  lua_close(L);
 }
 /* Every optional control uses the same atomic validation/persistence path. */
 lua_State*T=luaL_newstate();luaL_openlibs(T);
 run(T,"re={on_draw_ui=function(f) draw=f end}; changes={}; shown={}; imgui={tree_node=function() return true end,tree_pop=function() end,text=function() end,checkbox=function(s,v) shown[s]=v; return changes[s]~=nil,changes[s] end,combo=function(s,v,values) shown[s]=v; if s=='Custom Screen Shader' then for i=0,15 do assert(type(values[i])=='string') end else for i=-1,23 do assert(type(values[i])=='string') end end; return changes[s]~=nil,changes[s] end,slider_int=function(s,v,a,b) shown[s]=v; return changes[s]~=nil,changes[s] end,slider_float=function(s,v,a,b) shown[s]=v; return changes[s]~=nil,changes[s] end}");
 assert(native_options_ui_register(T));
 run(T,"draw(); assert(shown['Hide Mia Hair in First Person']); assert(not shown['Force Custom Screen Shader']); assert(shown['Custom Screen Shader']==1); assert(not shown['Fullbright']); assert(shown['Fullbright Brightness']==1); assert(shown['Controller Camera Toggle']==-1); changes={['Hide Mia Hair in First Person']=false,['Force Custom Screen Shader']=true,['Custom Screen Shader']=15,['Fullbright']=true,['Fullbright Red']=128,['Fullbright Green']=64,['Fullbright Blue']=32,['Fullbright Brightness']=2.5,['Controller Camera Toggle']=0,['Controller Shoulder Switch']=23}; draw()");
 NativeSettings settings;int status;assert(native_settings_get(&options,&settings,&status));
 assert(!settings.hide_mia_hair&&settings.custom_filter&&settings.shader==15&&settings.fullbright&&settings.rgb==0x804020&&settings.brightness==2.5f&&settings.camera_button==0&&settings.shoulder_button==23);
 run(T,"changes={['Custom Screen Shader']=16,['Controller Camera Toggle']=24,['Fullbright Brightness']=0/0}; draw()");
 assert(native_settings_get(&options,&settings,&status)&&settings.shader==15&&settings.camera_button==0&&settings.brightness==2.5f);lua_close(T);
 lua_State*L=luaL_newstate();luaL_openlibs(L);assert(!native_options_ui_register(L));assert(lua_gettop(L)==0);
 run(L,"_VERSION='Lua 5.3'");assert(!native_options_ui_register(L));lua_close(L);
 return 0;
}
