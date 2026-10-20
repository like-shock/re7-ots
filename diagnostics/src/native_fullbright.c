/* RT 8.02 CT15622-15909. All changes are register substitutions, apart from
 * the original ECX store to the game's stack. No persistent light writes. */
#include "native_runtime.h"
#include <math.h>
#include <string.h>
static int read_at(NativeRuntime*r,uint64_t b,uint64_t o,void*out,size_t n){
 const uint64_t max=UINT64_C(0x7fffffffffff);
 if(b<0x10000||b>max||o>max-b||n-1>max-b-o||!r->read||!r->read(r->context,b+o,out,n)){r->stats.reads_failed++;return 0;}return 1;
}
static void invalidate(NativeRuntime*r){
 r->fullbright.light=r->fullbright.player=r->fullbright.aim=0;
 r->fullbright.area=0;r->fullbright.shadow_override=0;
}
void native_fullbright_reset(NativeRuntime*r){invalidate(r);r->fullbright.observed=0;}
/* F6 OFF has no camera/FOV producers. Require the independent player, area
 * and loading observations instead; never borrow an old OTS-ready snapshot. */
static int first_person_ready(const NativeRuntime*r){
 const Re7OriginalInputs*i=&r->inputs;
 const uint64_t needed=RE7_INPUT_PLAYER|RE7_INPUT_AIM|RE7_INPUT_AREA|RE7_INPUT_TIMER;
 const uint64_t observed=(1ULL<<NS_CUTSCENE_AUTOLOOK)|(1ULL<<NS_LOADED_AREA)|(1ULL<<NS_TIMER_AOB);
 return (r->fullbright.observed&observed)==observed&&
  (i->state.loadarea==35||(r->fullbright.observed&(1ULL<<NS_BETTER_TIMER)))&&
  (i->valid&needed)==needed&&!(i->failed&needed)&&i->state.player_available&&
  i->player_base&&i->aim_address&&i->oldaim_address==i->aim_address&&
  i->state.loadscreenflag!=1&&isfinite(i->state.igtimer)&&i->state.igtimer>=1;
}
int native_runtime_set_fullbright(NativeRuntime*r,int e,uint32_t rgb,float b){
 if((e!=0&&e!=1)||rgb>0xffffff||!isfinite(b)||b<0||b>10)return 0;
 if(e!=r->fullbright.enabled)native_fullbright_reset(r);
 r->fullbright.enabled=(uint8_t)e;r->fullbright.rgb=rgb;r->fullbright.brightness=b;
 if(!e)native_fullbright_reset(r);
 return 1;
}
static int marker(NativeRuntime*r,uint64_t light){uint32_t value;return read_at(r,light,0x68,&value,4)&&value==0x48127c00u;}
static float color(unsigned byte,float brightness){
 int32_t value=(int32_t)byte;float result;const float unit=.003921568859f,scale=150000.f;
 __asm__ volatile("fildl %1; fmuls %2; fmuls %3; fmuls %4; fstps %0":"=m"(result):"m"(value),"m"(unit),"m"(brightness),"m"(scale):"st");return result;
}
static int diameter(NativeRuntime*r,NativeFrame*f,void*fp){
 float v[16];uint32_t ecx,identity;uint64_t stack=native_frame_game_rsp(f);
 /* CT retains the examine light while this shared site visits other lights.
  * Clearing it on every visit makes the radius consumers depend on ordering.
  * Unknown reads and a recycled target still revoke the cached identity. */
 if(!read_at(r,f->rax,0x68,&identity,4)){r->fullbright.light=0;return 0;}
 if(identity!=0x48127c00u){
  if(r->fullbright.light==f->rax)r->fullbright.light=0;
  return 0;
 }
 /* Stage every operand and recheck identity before the sole original store.
  * A failed store can safely replay: it writes the same original ECX value. */
 r->fullbright.light=0;
 if(!read_at(r,f->rax,0x80,v,sizeof v)||!read_at(r,f->rax,0xd0,&ecx,4)||!marker(r,f->rax))return 0;
 /* The original stack store precedes its last two vector loads. Reject
  * overlap instead of staging values that that store would have changed. */
 if(stack>UINT64_C(0x7fffffffff9f)||(stack+0x5c<f->rax+0xc0&&f->rax+0xa0<stack+0x60))return 0;
 if(stack<0x10000||!r->write||!r->write(r->context,stack+0x5c,&ecx,4)){r->stats.writes_failed++;return 0;}
 v[3]=1000;v[10]=-1.9f;v[12]=color((r->fullbright.rgb>>16)&255,r->fullbright.brightness);
 v[13]=color((r->fullbright.rgb>>8)&255,r->fullbright.brightness);v[14]=color(r->fullbright.rgb&255,r->fullbright.brightness);v[15]=-500;
 native_set_xmm_vector(fp,14,v);native_set_xmm_vector(fp,15,v+4);native_set_xmm_vector(fp,12,v+8);native_set_xmm_vector(fp,11,v+12);
 f->rcx=ecx;r->fullbright.light=f->rax;return 1;
}
static int flag_identity(NativeRuntime*r,uint64_t candidate){
 const unsigned offsets[]={0x8ff2848,0x150,0x50};uint64_t loc[3],links[3],current=r->base,next;
 for(unsigned j=0;j<3;j++){if(!read_at(r,current,offsets[j],&next,8))return 0;loc[j]=current+offsets[j];current=links[j]=next;}
 if(current>UINT64_MAX-0xf93||candidate!=current+0xf93)return 0;
 for(unsigned j=3;j>0;j--){if(!read_at(r,loc[j-1],0,&next,8)||next!=links[j-1])return 0;}
 return 1;
}
int native_fullbright_dispatch(NativeRuntime*r,unsigned site,NativeFrame*f,void*fp){
 if(!r->fullbright.enabled){native_fullbright_reset(r);return 0;}
 if(site==NS_CUTSCENE_AUTOLOOK||site==NS_LOADED_AREA||site==NS_TIMER_AOB||site==NS_BETTER_TIMER)
  r->fullbright.observed|=UINT64_C(1)<<site;
 if(r->camera_enabled?!r->inputs.ready:!first_person_ready(r)){invalidate(r);return 0;}
 if(!r->camera_enabled&&(r->fullbright.player!=r->inputs.player_base||
    r->fullbright.aim!=r->inputs.aim_address||r->fullbright.area!=r->inputs.state.loadarea)){
  invalidate(r);r->fullbright.player=r->inputs.player_base;
  r->fullbright.aim=r->inputs.aim_address;r->fullbright.area=r->inputs.state.loadarea;
 }
 switch(site){
 case NS_OLD_HOUSE_BRIGHTNESS_READ:
  if(r->fullbright.shadow_override){f->rax=0;return 1;}
  return 0;
 case NS_LIGHT_SOURCE_DIAMETER_READ:return diameter(r,f,fp);
 case NS_LIGHT_RADIUS_MULSS_READ:case NS_LIGHT_RADIUS_MOVSS_READ:
  if(!r->fullbright.light||f->rbx<0x80||f->rbx-0x80!=r->fullbright.light)return 0;
  if(!marker(r,r->fullbright.light)){r->fullbright.light=0;return 0;}
  if(site==NS_LIGHT_RADIUS_MOVSS_READ)native_set_xmm_scalar(fp,0,1000);
  else{float v[4];native_get_xmm_vector(fp,0,v);v[0]*=1000;native_set_xmm_vector(fp,0,v);}return 1;
 case NS_EXAMINE_LIGHT_CMP_1:case NS_EXAMINE_LIGHT_CMP_2:{
  uint64_t owner=site==NS_EXAMINE_LIGHT_CMP_1?f->rsi:f->rbx;
  if(owner>UINT64_MAX-0x13||!flag_identity(r,owner+0x13))return 0;
  /* Source pointer equality CMP has equal flags; handled continuation is
   * the validated fallthrough. Non-targets use the real original trampoline,
   * including CMP_2's actual +0x10 JNE, never the CT's contradictory address. */
  f->flags=(f->flags&~UINT64_C(0x8d5))|0x44;return 1;
 }
 case NS_GLOBAL_BRIGHTNESS:{
  float v[4];if(!read_at(r,f->r14,0x180,v,16)){r->fullbright.shadow_override=0;return 0;}
  v[0]=.125f;native_set_xmm_vector(fp,0,v);r->fullbright.shadow_override=1;return 1;
 }
 case NS_G_BRIGHTNESS_2:native_set_xmm_scalar(fp,10,.27f);return 1;
 default:return 0;
 }
}
