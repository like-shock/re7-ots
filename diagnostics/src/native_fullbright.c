/* RT 8.02 CT15622-15909. All changes are register substitutions, apart from
 * the original ECX store to the game's stack. No persistent light writes. */
#include "native_runtime.h"
#include <math.h>
#include <string.h>
static int read_at(NativeRuntime*r,uint64_t b,uint64_t o,void*out,size_t n){
 const uint64_t max=UINT64_C(0x7fffffffffff);
 if(b<0x10000||b>max||o>max-b||n-1>max-b-o||!r->read||!r->read(r->context,b+o,out,n)){r->stats.reads_failed++;return 0;}return 1;
}
static void note_failure(NativeRuntime*r,unsigned reason,unsigned site){
 NativeFullbrightTrace*t=&r->fullbright.trace;
 t->last_failure=(uint8_t)reason;t->failures[reason]++;
 t->failure_frame=r->inputs.frametrack;t->failure_site=site;
}
static void invalidate(NativeRuntime*r,unsigned reason,unsigned site){
 if(r->fullbright.light||r->fullbright.shadow_override)note_failure(r,reason,site);
 r->fullbright.light=r->fullbright.player=r->fullbright.aim=0;
 r->fullbright.area=0;r->fullbright.shadow_override=0;
}
void native_fullbright_reset(NativeRuntime*r){invalidate(r,NFB_FAILURE_RESET,UINT32_MAX);r->fullbright.observed=0;}
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
/* Explain the unchanged predicate; this does not authorize lighting. */
static unsigned gate_reason(const NativeRuntime*r){
 const Re7OriginalInputs*i=&r->inputs;
 if(!r->fullbright.enabled)return NFB_GATE_OFF;
 if(r->camera_enabled)return i->ready?NFB_GATE_READY:
  i->state.loadscreenflag==1?NFB_GATE_LOADING:NFB_GATE_CAMERA_INPUTS;
 if(first_person_ready(r))return NFB_GATE_READY;
 const uint64_t observed=(1ULL<<NS_CUTSCENE_AUTOLOOK)|(1ULL<<NS_LOADED_AREA)|(1ULL<<NS_TIMER_AOB);
 const uint64_t needed=RE7_INPUT_PLAYER|RE7_INPUT_AIM|RE7_INPUT_AREA|RE7_INPUT_TIMER;
 if((r->fullbright.observed&observed)!=observed||
    (i->state.loadarea!=35&&!(r->fullbright.observed&(1ULL<<NS_BETTER_TIMER))))return NFB_GATE_OBSERVERS;
 if((i->valid&needed)!=needed||(i->failed&needed))return NFB_GATE_INPUTS;
 if(!i->state.player_available||!i->player_base)return NFB_GATE_PLAYER;
 if(!i->aim_address||i->oldaim_address!=i->aim_address)return NFB_GATE_AIM_OWNER;
 if(i->state.loadscreenflag==1)return NFB_GATE_LOADING;
 return NFB_GATE_TIMER;
}
const char*native_fullbright_gate_name(unsigned reason){
 static const char*names[]={"off","ready","camera_inputs_not_ready","waiting_for_observers",
  "required_inputs_invalid","player_unavailable","aim_owner_unconfirmed","loading","timer_not_ready"};
 return reason<sizeof names/sizeof names[0]?names[reason]:"unknown";
}
const char*native_fullbright_failure_name(unsigned reason){
 static const char*names[]={"none","mode_or_option_reset","readiness_lost","owner_changed",
  "light_marker_read_failed","light_identity_changed","light_operands_or_identity_failed",
  "stack_overlap","stack_write_failed","radius_identity_failed","global_brightness_read_failed"};
 return reason<sizeof names/sizeof names[0]?names[reason]:"unknown";
}
void native_fullbright_snapshot(const NativeRuntime*r,NativeFullbrightEvent*out){
 *out=(NativeFullbrightEvent){
  .sequence=r->fullbright.trace.sequence,.light=r->fullbright.light,
  .player=r->inputs.player_base,.aim=r->inputs.aim_address,
  .input_valid=r->inputs.valid,.input_failed=r->inputs.failed,.observed=r->fullbright.observed,
  .frame=r->inputs.frametrack,.site=UINT32_MAX,.area=r->inputs.state.loadarea,
  .game_time=r->inputs.state.igtimer,.enabled=r->fullbright.enabled,.camera_enabled=r->camera_enabled,
  .firstperson=r->view.firstpersonflag,.stopflag=r->state.stopflag,.internalfp=r->state.internalfpflag,
  .shadow_override=r->fullbright.shadow_override,.gate=(uint8_t)gate_reason(r),
  .last_failure=r->fullbright.trace.last_failure
 };
}
void native_fullbright_record(NativeRuntime*r,unsigned site){
 NativeFullbrightTrace*t=&r->fullbright.trace;
 if(!r->fullbright.enabled&&(!t->have_last||!t->last.enabled))return;
 NativeFullbrightEvent e;native_fullbright_snapshot(r,&e);
 const NativeFullbrightEvent*p=&t->last;
 /* Time/counters alone do not create events. Periodic snapshots retain them. */
 if(t->have_last&&e.enabled==p->enabled&&e.camera_enabled==p->camera_enabled&&
    e.firstperson==p->firstperson&&e.stopflag==p->stopflag&&e.internalfp==p->internalfp&&
    e.gate==p->gate&&e.light==p->light&&e.shadow_override==p->shadow_override&&
    e.player==p->player&&e.aim==p->aim&&e.area==p->area&&
    e.input_valid==p->input_valid&&e.input_failed==p->input_failed&&e.last_failure==p->last_failure)return;
 e.site=site;e.sequence=++t->sequence;t->last=e;t->have_last=1;
 if(t->count==NATIVE_FULLBRIGHT_TRACE_CAPACITY){t->dropped++;return;}
 t->events[(t->head+t->count)%NATIVE_FULLBRIGHT_TRACE_CAPACITY]=e;t->count++;
}
unsigned native_fullbright_take_events(NativeRuntime*r,NativeFullbrightEvent*out,unsigned capacity){
 NativeFullbrightTrace*t=&r->fullbright.trace;
 unsigned n=t->count<capacity?t->count:capacity;
 for(unsigned j=0;j<n;j++)out[j]=t->events[(t->head+j)%NATIVE_FULLBRIGHT_TRACE_CAPACITY];
 t->head=(t->head+n)%NATIVE_FULLBRIGHT_TRACE_CAPACITY;t->count-=n;
 return n;
}
int native_runtime_set_fullbright(NativeRuntime*r,int e,uint32_t rgb,float b){
 if((e!=0&&e!=1)||rgb>0xffffff||!isfinite(b)||b<0||b>10)return 0;
 if(e!=r->fullbright.enabled)native_fullbright_reset(r);
 r->fullbright.enabled=(uint8_t)e;r->fullbright.rgb=rgb;r->fullbright.brightness=b;
 if(!e)native_fullbright_reset(r);
 native_fullbright_record(r,UINT32_MAX);
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
 if(!read_at(r,f->rax,0x68,&identity,4)){
  r->fullbright.light=0;note_failure(r,NFB_FAILURE_MARKER_READ,NS_LIGHT_SOURCE_DIAMETER_READ);return 0;
 }
 if(identity!=0x48127c00u){
  if(r->fullbright.light==f->rax){r->fullbright.light=0;note_failure(r,NFB_FAILURE_IDENTITY,NS_LIGHT_SOURCE_DIAMETER_READ);}
  return 0;
 }
 /* Stage every operand and recheck identity before the sole original store.
  * A failed store can safely replay: it writes the same original ECX value. */
 r->fullbright.light=0;
 if(!read_at(r,f->rax,0x80,v,sizeof v)||!read_at(r,f->rax,0xd0,&ecx,4)||!marker(r,f->rax)){
  note_failure(r,NFB_FAILURE_OPERANDS,NS_LIGHT_SOURCE_DIAMETER_READ);return 0;
 }
 /* The original stack store precedes its last two vector loads. Reject
  * overlap instead of staging values that that store would have changed. */
 if(stack>UINT64_C(0x7fffffffff9f)||(stack+0x5c<f->rax+0xc0&&f->rax+0xa0<stack+0x60)){
  note_failure(r,NFB_FAILURE_STACK_OVERLAP,NS_LIGHT_SOURCE_DIAMETER_READ);return 0;
 }
 if(stack<0x10000||!r->write||!r->write(r->context,stack+0x5c,&ecx,4)){
  r->stats.writes_failed++;note_failure(r,NFB_FAILURE_STACK_WRITE,NS_LIGHT_SOURCE_DIAMETER_READ);return 0;
 }
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
 if(r->camera_enabled?!r->inputs.ready:!first_person_ready(r)){invalidate(r,NFB_FAILURE_READINESS,site);return 0;}
 if(!r->camera_enabled&&(r->fullbright.player!=r->inputs.player_base||
    r->fullbright.aim!=r->inputs.aim_address||r->fullbright.area!=r->inputs.state.loadarea)){
  invalidate(r,NFB_FAILURE_OWNER,site);r->fullbright.player=r->inputs.player_base;
  r->fullbright.aim=r->inputs.aim_address;r->fullbright.area=r->inputs.state.loadarea;
 }
 switch(site){
 case NS_OLD_HOUSE_BRIGHTNESS_READ:
  if(r->fullbright.shadow_override){f->rax=0;return 1;}
  return 0;
 case NS_LIGHT_SOURCE_DIAMETER_READ:return diameter(r,f,fp);
 case NS_LIGHT_RADIUS_MULSS_READ:case NS_LIGHT_RADIUS_MOVSS_READ:
  if(!r->fullbright.light||f->rbx<0x80||f->rbx-0x80!=r->fullbright.light)return 0;
  if(!marker(r,r->fullbright.light)){r->fullbright.light=0;note_failure(r,NFB_FAILURE_RADIUS_IDENTITY,site);return 0;}
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
  float v[4];if(!read_at(r,f->r14,0x180,v,16)){
   r->fullbright.shadow_override=0;note_failure(r,NFB_FAILURE_GLOBAL_READ,site);return 0;
  }
  v[0]=.125f;native_set_xmm_vector(fp,0,v);r->fullbright.shadow_override=1;return 1;
 }
 case NS_G_BRIGHTNESS_2:native_set_xmm_scalar(fp,10,.27f);return 1;
 default:return 0;
 }
}
