/* Same pinned Lua-core linkage as the existing REFramework native bridge.
 * Embedded menu only: no on_frame callback, SDK traversal or camera writes. */
#include <string.h>
#include "lua.h"
#include "native_options_ui.h"
#include "native_options.h"
_Static_assert(sizeof(lua_Integer)==8&&sizeof(lua_Number)==8,"REFramework Lua numeric ABI");
static const char menu[]=
 "local api=...\n"
 "assert(type(re)=='table' and type(re.on_draw_ui)=='function')\n"
 "assert(type(imgui)=='table' and type(imgui.tree_node)=='function' and type(imgui.checkbox)=='function' and type(imgui.text)=='function' and type(imgui.tree_pop)=='function' and type(imgui.combo)=='function' and type(imgui.slider_int)=='function' and type(imgui.slider_float)=='function')\n"
 "local shaders={[0]='Critical Health',[1]='Main Menu',[2]='Death Grayscale',[3]='Birthday VHS',[4]='Flashback',[5]='Unnamed 5',[6]='Mia VHS',[7]='Old House',[8]='Testing Area',[9]='Guest House',[10]='Wrecked Ship',[11]='Docks',[12]='Lucas Room',[13]='Safe Room',[14]='Baker House',[15]='Outside Finale'}\n"
 "local buttons={[-1]='Unassigned',[0]='D-pad Up',[1]='D-pad Right',[2]='D-pad Down',[3]='D-pad Left',[4]='Y',[5]='B',[6]='A',[7]='X',[8]='Left Shoulder',[9]='Left Trigger',[10]='Right Shoulder',[11]='Right Trigger',[12]='Left Stick Click',[13]='Right Stick Click',[14]='View',[15]='Menu',[16]='Left Stick Up',[17]='Left Stick Right',[18]='Left Stick Down',[19]='Left Stick Left',[20]='Right Stick Up',[21]='Right Stick Right',[22]='Right Stick Down',[23]='Right Stick Left'}\n"
 "re.on_draw_ui(function()\n"
 " if imgui.tree_node('RE7 OTS Native') then\n"
 "  local ready,melee,head,contrast,status,hair,filter,shader,bright,rgb,power,cam,shoulder,distance,side,height,customfov,fov,runfp,runtp,runmax,runrise,runfall=api.get()\n"
 "  if ready then\n"
 "   local changed,new_value=imgui.checkbox('Auto-Disable with Melee Weapons',melee)\n"
 "   if changed then api.set(1,new_value) end\n"
 "   imgui.text('OFF keeps third person for melee weapons. Other automatic transitions still apply.')\n"
 "   changed,new_value=imgui.checkbox('Hide Head in First Person',head)\n"
 "   if changed then api.set(2,new_value) end\n"
 "   imgui.text('Also applies with F6 OFF. Disable to compare the original head rendering.')\n"
 "   changed,new_value=imgui.checkbox('Limit Light Source Contrast in Third Person',contrast)\n"
 "   if changed then api.set(4,new_value) end\n"
 "   imgui.text('Limits light-driven darkening in third person. Compare ON/OFF at the same camera angle.')\n"
 "   changed,new_value=imgui.checkbox('Hide Mia Hair in First Person',hair)\n"
 "   if changed then api.set(8,new_value) end\n"
 "   imgui.text('Requires Hide Head in First Person. Turn off for incompatible custom Mia hair.')\n"
 "   changed,new_value=imgui.checkbox('Force Custom Screen Shader',filter)\n"
 "   if changed then api.set(9,new_value) end\n"
 "   changed,new_value=imgui.combo('Custom Screen Shader',shader,shaders)\n"
 "   if changed then api.set(10,new_value) end\n"
 "   imgui.text('Critical-health shader takes priority. Custom shader applies while F6 is ON.')\n"
 "   imgui.text('Fullbright also applies in first person with F6 OFF. Uncheck to restore normal lighting.')\n"
 "   changed,new_value=imgui.checkbox('Fullbright',bright)\n"
 "   if changed then api.set(11,new_value) end\n"
 "   local red=(rgb>>16)&255; local green=(rgb>>8)&255; local blue=rgb&255\n"
 "   changed,new_value=imgui.slider_int('Fullbright Red',red,0,255)\n"
 "   if changed then red=new_value; api.set(12,(red<<16)|(green<<8)|blue) end\n"
 "   changed,new_value=imgui.slider_int('Fullbright Green',green,0,255)\n"
 "   if changed then green=new_value; api.set(12,(red<<16)|(green<<8)|blue) end\n"
 "   changed,new_value=imgui.slider_int('Fullbright Blue',blue,0,255)\n"
 "   if changed then blue=new_value; api.set(12,(red<<16)|(green<<8)|blue) end\n"
 "   changed,new_value=imgui.slider_float('Fullbright Brightness',power,0,10)\n"
 "   if changed then api.set(13,new_value) end\n"
 "   changed,new_value=imgui.combo('Controller Camera Toggle',cam,buttons)\n"
 "   if changed then api.set(14,new_value) end\n"
 "   changed,new_value=imgui.combo('Controller Shoulder Switch',shoulder,buttons)\n"
 "   if changed then api.set(15,new_value) end\n"
 "   imgui.text('Unassigned by default. Release then press after binding, loading or reconnecting. Camera toggle works with F6 OFF.')\n"
 "   imgui.text('Camera defaults: distance 1, shoulder 1, height 0. F7 retains the chosen shoulder scale.')\n"
 "   changed,new_value=imgui.slider_float('Camera Distance Scale',distance,.5,2)\n"
 "   if changed then api.set(16,new_value) end\n"
 "   changed,new_value=imgui.slider_float('Shoulder Offset Scale',side,0,2)\n"
 "   if changed then api.set(17,new_value) end\n"
 "   changed,new_value=imgui.slider_float('Camera Height Offset',height,-.5,.5)\n"
 "   if changed then api.set(18,new_value) end\n"
 "   imgui.text('FOV options apply with F6 ON. F6 OFF returns FOV control to the game / REFramework.')\n"
 "   changed,new_value=imgui.checkbox('Use Custom FOV',customfov)\n"
 "   if changed then api.set(19,new_value) end\n"
 "   changed,new_value=imgui.slider_float('Custom FOV',fov,60,120)\n"
 "   if changed then api.set(20,new_value) end\n"
 "   changed,new_value=imgui.checkbox('First-Person Running FOV',runfp)\n"
 "   if changed then api.set(21,new_value) end\n"
 "   changed,new_value=imgui.checkbox('Third-Person Running FOV',runtp)\n"
 "   if changed then api.set(22,new_value) end\n"
 "   changed,new_value=imgui.slider_float('Running FOV Increase (%)',runmax,0,40)\n"
 "   if changed then api.set(23,new_value) end\n"
 "   changed,new_value=imgui.slider_float('Running FOV Rise (seconds)',runrise,.1,10)\n"
 "   if changed then api.set(24,new_value) end\n"
 "   changed,new_value=imgui.slider_float('Running FOV Fall (seconds)',runfall,.1,5)\n"
 "   if changed then api.set(25,new_value) end\n"
 "   if status==-1 then imgui.text('Could not save. This change applies to this session only.')\n"
 "   elseif status==0 then imgui.text('Saving setting...') end\n"
 "  else imgui.text('Native settings are not ready.') end\n"
 "  imgui.tree_pop()\n"
 " end\n"
 "end)\n";
static int get(lua_State*L){
 NativeSettings s=native_settings_defaults();int status=0,ready=native_options_ui_get_settings(&s,&status);
 lua_pushboolean(L,ready);lua_pushboolean(L,s.flags&NATIVE_OPTION_MELEE);lua_pushboolean(L,s.flags&NATIVE_OPTION_HIDE_HEAD);lua_pushboolean(L,s.flags&NATIVE_OPTION_LIGHT_CONTRAST);lua_pushinteger(L,status);
 lua_pushboolean(L,s.hide_mia_hair);lua_pushboolean(L,s.custom_filter);lua_pushinteger(L,s.shader);lua_pushboolean(L,s.fullbright);lua_pushinteger(L,s.rgb);lua_pushnumber(L,s.brightness);lua_pushinteger(L,s.camera_button);lua_pushinteger(L,s.shoulder_button);
 lua_pushnumber(L,s.camera_distance);lua_pushnumber(L,s.camera_shoulder);lua_pushnumber(L,s.camera_height);
 lua_pushboolean(L,s.use_custom_fov);lua_pushnumber(L,s.custom_fov);lua_pushboolean(L,s.fp_run_fov);lua_pushboolean(L,s.tp_run_fov);
 lua_pushnumber(L,s.run_fov_amount);lua_pushnumber(L,s.run_fov_rise);lua_pushnumber(L,s.run_fov_fall);return 23;
}
static int set(lua_State*L){
 int ok=0;
 if(lua_isinteger(L,1)){
  lua_Integer id=lua_tointeger(L,1);
  if(id==1||id==2||id==4)ok=lua_type(L,2)==LUA_TBOOLEAN&&native_options_ui_set((unsigned)id,lua_toboolean(L,2));
  else if(id==8||id==9||id==11||id==19||id==21||id==22)ok=lua_type(L,2)==LUA_TBOOLEAN&&native_options_ui_set_setting((unsigned)id,lua_toboolean(L,2));
  else if(id>=10&&id<=25)ok=lua_type(L,2)==LUA_TNUMBER&&native_options_ui_set_setting((unsigned)id,lua_tonumber(L,2));
 }
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
