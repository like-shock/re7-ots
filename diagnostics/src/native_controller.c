/* Source CT3829-3960: producers collect exactly 24 source button bytes.
 * They always replay both complete original spans; the worker alone consumes
 * queued rising edges and changes camera/shoulder lifecycle. */
#include "native_runtime.h"
#include <string.h>
int native_controller_configured(const NativeRuntime*r){return r->controller.camera_button>=0||r->controller.shoulder_button>=0;}
void native_controller_reset(NativeRuntime*r){
 NativeControllerState*c=&r->controller;
 /* Configuration/connection fields are worker-owned and must not be written
  * by hook-side invalidation, even with the same value. */
 c->armed=c->down=c->pending_camera=c->pending_shoulder=0;
 memset(c->owner,0,sizeof c->owner);memset(c->samples,0,sizeof c->samples);
 memset(c->seen,0,sizeof c->seen);memset(c->last,0,sizeof c->last);
 c->aim=0;c->area=0;
}
int native_runtime_set_controller_bindings(NativeRuntime*r,int c,int s){
 if(c< -1||c>23||s< -1||s>23)return 0;
 if(c!=r->controller.camera_button||s!=r->controller.shoulder_button){r->controller.camera_button=c;r->controller.shoulder_button=s;native_controller_reset(r);}
 return 1;
}
void native_controller_take(NativeRuntime*r,unsigned*c,unsigned*s){
 *c=r->controller.pending_camera;*s=r->controller.pending_shoulder;
 r->controller.pending_camera=r->controller.pending_shoulder=0;
}
void native_controller_poll(NativeRuntime*r,uint64_t now){
 for(unsigned j=0;j<2;j++){
  NativeControllerState*c=&r->controller;
  if(c->samples[j]!=c->seen[j]){c->seen[j]=c->samples[j];c->last[j]=now;}
  else if(c->owner[j]&&now-c->last[j]>=500){
   uint32_t mask=j?0xff0000u:0xffffu;c->armed&=~mask;c->down&=~mask;c->owner[j]=0;
   c->pending_camera=c->pending_shoulder=0;
  }
 }
}
static int read_at(NativeRuntime*r,uint64_t b,uint64_t o,void*out,size_t n){
 const uint64_t max=UINT64_C(0x7fffffffffff);
 if(b<0x10000||b>max||o>max-b||n-1>max-b-o||!r->read||!r->read(r->context,b+o,out,n)){r->stats.reads_failed++;return 0;}return 1;
}
int native_controller_dispatch(NativeRuntime*r,unsigned site,NativeFrame*f,void*fp){
 (void)fp;if(!native_controller_configured(r))return 0;
 NativeControllerState*c=&r->controller;
 const uint64_t needed=RE7_INPUT_AREA|RE7_INPUT_AIM|RE7_INPUT_TIMER;
 const uint64_t observed=(1ULL<<NS_CUTSCENE_AUTOLOOK)|(1ULL<<NS_LOADED_AREA);
 if(!c->connection_available||!c->connected_mask||(r->inputs.observed&observed)!=observed||!(r->inputs.observed&((1ULL<<NS_TIMER_AOB)|(1ULL<<NS_BETTER_TIMER)))||(r->inputs.valid&needed)!=needed||(r->inputs.failed&needed)||!r->inputs.aim_address||r->inputs.state.loadscreenflag==1){native_controller_reset(r);return 0;}
 if(c->aim!=r->inputs.aim_address||c->area!=r->inputs.state.loadarea){native_controller_reset(r);c->aim=r->inputs.aim_address;c->area=r->inputs.state.loadarea;}
 if(site!=NS_XINPUT_FLOATS&&site!=NS_XINPUT_WRITE)return 0;
 unsigned group=site==NS_XINPUT_WRITE;uint64_t owner=group?f->rbx:f->rax;
 uint32_t mask=group?0xff0000u:0xffffu,down=0;
 if(group){
  uint32_t original; /* The exact WRITE span will perform this read too. */
  if(!read_at(r,owner,0xa0,&original,4)){native_controller_reset(r);return 0;}
  unsigned left=((uint32_t)f->rax>>16)&255,right=((uint32_t)f->rax>>24)&255;
  for(unsigned j=0;j<4;j++){if(left==(0x10u<<j))down|=1u<<(16+j);if(right==(1u<<j))down|=1u<<(20+j);}
 }else{
  uint32_t value[16];
  if(!read_at(r,owner,0x24,value,sizeof value)){native_controller_reset(r);return 0;}
  for(unsigned j=0;j<16;j++){if(value[j]!=0)down|=1u<<j;}
 }
 if(c->owner[group]!=owner){
  /* Queues have no producer identity; replacement invalidates both actions.
   * First capture has no stale owner and must preserve the other group. */
  if(c->owner[group])c->pending_camera=c->pending_shoulder=0;
  c->armed&=~mask;c->down&=~mask;c->owner[group]=owner;
 }
 uint32_t rising=down&~c->down&c->armed;
 c->armed|=(~down)&mask;c->down=(c->down&~mask)|down;c->samples[group]++;
 if(c->camera_button>=0&&(rising&(1u<<c->camera_button)))c->pending_camera++;
 if(c->shoulder_button>=0&&(rising&(1u<<c->shoulder_button)))c->pending_shoulder++;
 return 0;
}

void native_controller_connections(NativeRuntime*r,int available,unsigned mask){
 available=available==1&&mask<=15;mask=available?mask:0;
 if(available!=r->controller.connection_available||mask!=r->controller.connected_mask)native_controller_reset(r);
 r->controller.connection_available=available;r->controller.connected_mask=mask;
}
