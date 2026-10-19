/* RT 8.02 screens: CT1213-1217,3271-3279,4007-4030,9225-9759,10048-10114.
 * Defaults are source
 * fpvignette/chromaticfp=1, TP=0; psycho FP=1/TP=2; NAH TP redline disabled.
 * Zero returns replay the complete original span with an untouched frame. */
#include "native_runtime.h"
#include <float.h>
#include <string.h>
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
 r->screen=(NativeScreenState){0};r->state.stoppedbloodhide=0;
 r->interaction_input.scrapexamine=r->config.critterexamine=0;
}

/* CT1213-1217: re7+8FBB610 -> A0 -> D0 -> 70 -> HP+14.
 * Validate all link identities twice, then repeat around the HP sample. A
 * cached value never grants control; this rejects replacement, not ABA. */
static int hp_chain(NativeRuntime*r,uint64_t*out){
 const uint64_t maximum=UINT64_C(0x7fffffffffff);
 const uint32_t offsets[]={0x8fbb610,0xa0,0xd0,0x70};
 uint64_t locations[4],links[4],current=r->base,next;
 for(unsigned j=0;j<4;j++){
  if(current<0x10000||current>maximum||offsets[j]>maximum-current||
     current+offsets[j]>maximum-7)return 0;
  locations[j]=current+offsets[j];
  if(!read_at(r,locations[j],0,&next,8)||next<0x10000||next>maximum)return 0;
  current=links[j]=next;
 }
 for(unsigned j=4;j>0;j--)
  if(!read_at(r,locations[j-1],0,&next,8)||next!=links[j-1])return 0;
 if(current>maximum-0x17)return 0;
 *out=current+0x14;return 1;
}
void native_screen_observe_player(NativeRuntime*r){
 uint64_t address=0;
 if(r->camera_enabled&&r->inputs.ready)hp_chain(r,&address);
 r->screen.hp_address=address;
}
void native_screen_tick(NativeRuntime*r){
 if(!r->camera_enabled||!r->inputs.ready){native_screen_reset(r);return;}
 if(r->state.stoppedbloodhide==1)r->screen.stoppedbloodheaddelay=2;
 else if(r->screen.stoppedbloodheaddelay)r->screen.stoppedbloodheaddelay--;
}
static int blood(NativeRuntime*r,void*fp){
 if(r->state.stoppedbloodhide!=1){
  uint64_t address,confirmed;float hp,divided;uint32_t value;const float ten=10.f;
  if(!r->screen.hp_address||!hp_chain(r,&address)||address!=r->screen.hp_address||
     !read_at(r,address,0,&hp,4)||!hp_chain(r,&confirmed)||confirmed!=address){
   r->screen.hp_address=0;return 0;
  }
  /* Exact source FLD/FDIV/FSTP rounding, then unsigned DWORD CMP/JBE.
   * Incidental injected EAX/scratch flags are private: the original span is
   * MOVSS, whose game-visible GPRs and flags remain unchanged. */
  __asm__ volatile("flds %1; fdivs %2; fstps %0":"=m"(divided):"m"(hp),"m"(ten):"st");
  memcpy(&value,&divided,4);
  uint32_t limit=r->view.firstpersonflag==1?0x42c80000u:0;
  if(limit>value)return 0;
 }
 native_set_xmm_scalar(fp,0,0.f);return 1;
}
static int hurt(NativeRuntime*r,NativeFrame*f){
 uint32_t value;
 if(!read_at(r,f->rsi,0x18,&value,4)){
  r->screen.hurtfilterframes=r->screen.attemptedfilter=0;return 0;
 }
 if(value==0){
  value=r->view.firstpersonflag==1?0:2;
  r->screen.attemptedfilter=98;r->screen.hurtfilterframes=3;
 }else{
  if(value!=UINT32_MAX&&!r->screen.hurtfilterframes)r->screen.attemptedfilter=value;
  if(r->screen.hurtfilterframes)r->screen.hurtfilterframes--;
 }
 /* Complete eight-byte span: original MOV R8D zero-extends. Every source
  * return substitutes CMP AX=1,0; only its six arithmetic flags change. */
 f->r8=value;f->flags&=~UINT64_C(0x8d5);return 1;
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
static int custom_filter(NativeRuntime*r,unsigned site,NativeFrame*f){
 if(r->screen.hurtfilterframes)return 0;
 uint32_t original,other;int second=site==NS_FLASHBACK_FILTER_READ_2;
 if(!read_at(r,second?f->rcx:f->rdi,second?0x28:0x64,&original,4))return 0;
 if(!r->custom_filter){r->screen.attemptedfilter=original;return 0;}
 if(!read_at(r,f->rcx,second?0x38:0x14,&other,4))return 0;
 uint32_t left=second?r->custom_shader:other,right=second?other:r->custom_shader;
 uint64_t flags;
 __asm__ volatile("cmpl %2,%1; pushfq; popq %0":"=r"(flags):"r"(left),"r"(right):"cc");
 f->rax=r->custom_shader;f->flags=(f->flags&~UINT64_C(0x8d5))|(flags&0x8d5);
 r->screen.attemptedfilter=99;return 1;
}
int native_screen_dispatch(NativeRuntime*r,unsigned site,NativeFrame*f,void*fp){
 if(!r->camera_enabled||!r->inputs.ready)return 0;
 switch(site){
 case NS_FLASHBACK_FILTER_READ:case NS_FLASHBACK_FILTER_READ_2:return custom_filter(r,site,f);
 case NS_SCREEN_BLOOD_WRITE_FOLLOW:return blood(r,fp);
 case NS_HURT_FILTER_READ:return hurt(r,f);
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
