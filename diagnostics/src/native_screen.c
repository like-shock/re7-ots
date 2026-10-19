/* RT 8.02 default screens, CT4007-4030 and CT9629-9759. Defaults are source
 * fpvignette/chromaticfp=1, TP=0; psycho FP=1/TP=2; NAH TP redline disabled.
 * Zero returns replay the complete original span with an untouched frame. */
#include "native_runtime.h"
#include <float.h>
#if LDBL_MANT_DIG != 64
#error "RT 8.02 psycho intensity requires x87 extended precision"
#endif
static int read_at(NativeRuntime*r,uint64_t base,uint64_t offset,void*out,size_t n){
 if(base>UINT64_MAX-offset||base+offset>UINT64_MAX-(n-1)||
    !r->read||!r->read(r->context,base+offset,out,n)){
  r->stats.reads_failed++;return 0;
 }
 return 1;
}
static int read_cb(void*context,uint64_t address,void*out,size_t n){
 return read_at(context,address,0,out,n);
}
void native_screen_reset(NativeRuntime*r){
 r->screen=(NativeScreenState){0};
 r->interaction_input.scrapexamine=r->config.critterexamine=0;
}
static float intensity(float input,float multiplier){
 /* Exact FLD/FMUL/FSTP boundary, including source nonfinite behavior. The
  * bridge establishes masked round-to-nearest independently of game state. */
 float output;
 __asm__ volatile("flds %1; fmuls %2; fstps %0":"=m"(output):"m"(input),"m"(multiplier):"st");
 return output;
}
static int distortion(NativeRuntime*r,NativeFrame*f,void*fp){
 uint64_t target;
 if(!read_at(r,f->rcx,0x18,&target,8)){r->screen.psychoactive=0;return 0;}
 if(r->screen.psychoactive)r->screen.psychoactive--;
 if(!r->screen.psychoactive&&r->view.firstpersonflag==1)return 0;
 float result=r->screen.psychoactive?
  intensity(native_get_xmm_scalar(fp,1),r->view.firstpersonflag==1?1.f:2.f):0.f;
 /* Both replacement MOVSS loads clear upper 96 bits before original MOVAPS.
  * The ordinary FP path instead replays, preserving XMM1's existing lanes. */
 native_set_xmm_scalar(fp,1,result);
 const float vector[4]={result,0,0,0};native_set_xmm_vector(fp,2,vector);
 f->rax=target;return 1;
}
int native_screen_dispatch(NativeRuntime*r,unsigned site,NativeFrame*f,void*fp){
 if(!r->camera_enabled||!r->inputs.ready)return 0;
 switch(site){
 case NS_VIGNETTE:
  if(r->view.firstpersonflag==1)return 0;
  f->rax=2;return 1; /* MOV EAX,2 zero-extends RAX. */
 case NS_PSYCHO_TIMER:{
  float value;
  if(!read_at(r,f->rbx,0x240,&value,4)){r->screen.psychoactive=0;return 0;}
  r->screen.psychoactive=2;
  native_set_xmm_scalar(fp,0,value);return 1;
 }
 case NS_CLOSE_DISTORTION:return distortion(r,f,fp);
 case NS_NAH_DOT_FORCE:{
  uint8_t aim;
  if(r->inputs.state.loadarea!=35||r->view.firstpersonflag==1||
     !re7_original_inputs_live_aim(&r->inputs,r->base,read_cb,r,&aim)||aim!=1)return 0;
  f->rbp=0;f->rax=(uint32_t)(f->r14-0x31);return 1;
 }
 case NS_NAH_CRIT_HP_EFFECT_CMP:
  if(r->view.firstpersonflag==1)return 0;
  /* CMP AL,AL defines OF/SF/ZF/AF/PF/CF; all other flags stay intact. */
  f->flags=(f->flags&~UINT64_C(0x8d5))|0x44;return 1;
 default:return 0;
 }
}
