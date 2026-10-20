/* RT 8.02 default audiovisual corrections. Source lines refer to the private
 * CT hash recorded in the plan/NOTICE. No settings/SDK object discovery. */
#include "native_runtime.h"
#include <string.h>
#include <float.h>
#if LDBL_MANT_DIG != 64
#error "RT 8.02 flashlight math requires x87 extended precision"
#endif
static uint32_t bits(float v){uint32_t b;memcpy(&b,&v,4);return b;}
static int finite_value(float v){return (bits(v)&0x7f800000u)!=0x7f800000u;}
static float store32(long double v){volatile float result=(float)v;return result;}
static int add(uint64_t a,uint64_t d,uint64_t*out){if(a>UINT64_MAX-d)return 0;*out=a+d;return 1;}
static int read_at(NativeRuntime*r,uint64_t a,uint64_t d,void*out,size_t n){
 uint64_t p;if(!add(a,d,&p)||p>UINT64_MAX-n||!r->read||!r->read(r->context,p,out,n)){r->stats.reads_failed++;return 0;}return 1;
}
static int write_at(NativeRuntime*r,uint64_t a,uint64_t d,const void*in,size_t n){
 uint64_t p;if(!add(a,d,&p)||p>UINT64_MAX-n||!r->write||!r->write(r->context,p,in,n)){r->stats.writes_failed++;return 0;}return 1;
}
void native_av_reset(NativeRuntime*r){
 /* Every readiness/F6 boundary revokes effect authority immediately. Shared
  * character/flashlight inputs still serve independently enabled head hiding. */
 r->camera_valid=0;r->camera_base=0;r->camera_attempt=(Re7OriginalVec){0};
 r->inputs.state.camera=(Re7OriginalVec){0};r->yawx=r->yawz=r->pitch=0;
 r->av=(NativeAvState){0};
}
int native_av_flash_target(const NativeRuntime*r,uint64_t target,uint64_t delta){
 return r->av.flashlight_address&&r->av.flashlight_address==r->inputs.flashlight_address&&
  (r->inputs.observed&(1ULL<<NS_FLASHLIGHT_WRITE))&&
  (r->inputs.valid&RE7_INPUT_FLASHLIGHT)&&r->inputs.flashlight_address>=delta&&
  target==r->inputs.flashlight_address-delta;
}
void native_av_tick(NativeRuntime*r){
 /* TIMESCALE_MULTIPLIER L3258-3270, at source cadence (not render count). */
 if(r->state.readjustxyzfp==0)r->av.transition_frames=0;
 if(r->state.readjustxyzfp==2&&r->av.transition_frames<5)r->av.transition_frames++;
}
static int flashlight_position(NativeRuntime*r,NativeFrame*f,void*fp){
 /* L9762-9873. Both original transition exceptions precede FP rejection. */
 if(!r->camera_valid||!r->camera_base||f->rsi<0x40)return 0;
 if(r->state.gbunzoomingprocessflag!=1&&
    !(r->state.readjustxyzfp==2&&r->av.transition_frames<=1)&&r->view.firstpersonflag==1)return 0;
 uint32_t red;uint64_t target=f->rsi-0x40;
 if(!read_at(r,target,0,&red,4))return 0;
 if(red!=bits(.9098039269f)&&target!=r->av.alt_flash_red_address)return 0;
 float yawx,yawz;
 if(!read_at(r,r->camera_base,8,&yawx,4)||!read_at(r,r->camera_base,0,&yawz,4)||
    !finite_value(yawx)||!finite_value(yawz))return 0;
 const Re7OriginalParams*p=&r->params;
 /* Default child entry L27613 overrides initial allocation 0 to 0.2.
  * FSTP boundaries and operation order match the CT, with NO clip factors. */
 float backx=store32((long double)yawx*-1.f*.2f*p->backdisadjust*p->behinddis*p->aimbehinddis*p->pitchmul);
 float sidex=store32((long double)yawz*p->sidedisadjust*p->sidedis*p->aimsidedis);
 float backz=store32((long double)yawz*-1.f*.2f*p->backdisadjust*p->behinddis*p->aimbehinddis*p->pitchmul);
 float sidez=store32((long double)yawx*p->sidedisadjust*p->sidedis*p->aimsidedis);
 float out[]={store32((long double)r->camera_attempt.x-backx+sidex),
              store32((long double)r->camera_attempt.y+(-.2f)),
              store32((long double)r->camera_attempt.z+backz+sidez)};
 if(!finite_value(out[0])||!finite_value(out[1])||!finite_value(out[2]))return 0;
 if(!write_at(r,f->rsi,0,out,sizeof out))return 0;
 float vector[4];native_get_xmm_vector(fp,8,vector);native_set_xmm_vector(fp,0,vector);
 r->av.alt_flash_red_address=target;return 1;
}
static float color_factor(float channel,float brightness){
 /* L9904-9966: float(1/255), FISTP signed integer, then FILD and two
  * FMULs before the FSTP. The bridge establishes masked round-to-nearest. */
 const float step=0x1.010102p-8f;int32_t quantized;
 __asm__ volatile("flds %1; fdivs %2; fistpl %0":"=m"(quantized):"m"(channel),"m"(step):"st");
 return store32((long double)quantized*step*brightness);
}
static int flashlight_color(NativeRuntime*r,NativeFrame*f,void*fp){
 if(!native_av_flash_target(r,f->rax,0x4d0))return 0;
 float rgb[3],x0[4],x2[4],x1[4],zero[4]={0};
 if(!read_at(r,f->rax,0,rgb,sizeof rgb))return 0;
 native_get_xmm_vector(fp,0,x0);native_get_xmm_vector(fp,2,x2);memcpy(x1,x2,sizeof x1);
 float brightness=r->view.firstpersonflag==1?1.f:2.f;
 /* Legacy MULSS preserves upper lanes; MOVAPS copies all four lanes. */
 x0[0]*=color_factor(rgb[0],brightness);x1[0]*=color_factor(rgb[1],brightness);x2[0]*=color_factor(rgb[2],brightness);
 native_set_xmm_vector(fp,0,x0);native_set_xmm_vector(fp,1,x1);
 native_set_xmm_vector(fp,2,x2);native_set_xmm_vector(fp,11,zero);return 1;
}
static int audio_coords(NativeRuntime*r,NativeFrame*f){
 /* L3963-4004. Raw engine camera attempts, including FP; ECX/EAX still
  * receive source Y/Z. Avoid aliases whose reads depend on earlier stores. */
 if(!r->camera_valid||f->rbx>UINT64_MAX/8)return 0;
 uint64_t dst,src;
 if(!add(f->rsi,f->rbx*8,&dst)||!add(f->rax,4,&src)||src>UINT64_MAX-8||dst>UINT64_MAX-12||
    !(src+8<=dst||dst+12<=src))return 0;
 uint32_t yz[2];if(!read_at(r,src,0,yz,sizeof yz))return 0;
 if(!finite_value(r->camera_attempt.x)||!finite_value(r->camera_attempt.y)||!finite_value(r->camera_attempt.z))return 0;
 if(!write_at(r,dst,0,&r->camera_attempt,sizeof r->camera_attempt))return 0;
 f->rcx=yz[0];f->rax=yz[1];return 1;
}
static int vhs_boxes(NativeRuntime*r,NativeFrame*f){
 /* L2389-2437. Hide only matching boxes; the source skips the other 12
  * bytes of MOVUPS rather than rewriting them from XMM1. */
 int spider=r->state.spiderpuzfpvhsexc==1&&r->config.thirdpersonflag!=0&&r->inputs.state.loadarea==10;
 if(!spider&&r->view.firstpersonflag==1)return 0;
 if(!(r->inputs.valid&RE7_INPUT_CHARACTER)||(r->inputs.pcharcflag!=3&&r->inputs.pcharcflag!=4))return 0;
 uint32_t v;if(!read_at(r,f->r15,0x168,&v,4))return 0;
 if(v!=0x3f2aaaabu){
  if(!read_at(r,f->r15,0x128,&v,4)||v!=0x3f800000u)return 0;
  if(!read_at(r,f->r15,0x10c,&v,4))return 0;
  if(v==0&&(!read_at(r,f->r15,0xf0,&v,4)||v==0))return 0;
 }
 const float y=-1000.f;return write_at(r,f->r15,0x11c,&y,4);
}
int native_av_dispatch(NativeRuntime*r,unsigned site,NativeFrame*f,void*fp){
 if(!r->camera_enabled||!r->inputs.ready)return 0;
 switch(site){
 case NS_FLASHLIGHT_STEADY_XMM:{
  if(r->view.firstpersonflag==1)return 0;
  const float v[]={1,1,1,1};native_set_xmm_vector(fp,2,v);return 1;
 }
 case NS_FLASHLIGHT_ALT_WRITE:return flashlight_position(r,f,fp);
 case NS_FLASHLIGHT_COLORS_FLOAT_READS:return flashlight_color(r,f,fp);
 case NS_FLASH_BEAM_WIDTH:{
  uint64_t target;if(r->view.firstpersonflag==1||!add(f->rdi,0xac,&target)||!native_av_flash_target(r,target,0x464))return 0;
  const float width=10;return write_at(r,target,0,&width,4);
 }
 case NS_CAM_SOUND_COORDS_WRITE:return audio_coords(r,f);
 case NS_HEAD_BOB_FLAG_CMP:
  if(r->view.firstpersonflag==1)return 0;
  /* CMP AX,AX; handled continuation is the original JE target, NOT +9. */
  f->flags=(f->flags&~UINT64_C(0x8d5))|0x44;return 1;
 case NS_DOF_CUTSCENE_BLUR_READ:
  if(r->view.firstpersonflag==1||r->inputs.state.loadarea==3)return 0;
  native_set_xmm_scalar(fp,9,0);return 1;
 case NS_CAM_BOXES_WRITE:return vhs_boxes(r,f);
 default:return 0;
 }
}
