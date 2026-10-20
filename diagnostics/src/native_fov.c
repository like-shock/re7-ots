/* RT8.02 TRUE_FOV_READ follows the existing camera state_frame calculations.
 * Every f32 is an explicit source FSTP boundary; comparisons use DWORD bits. */
#include "native_runtime.h"
#include <math.h>
#include <string.h>
static uint32_t bits(float f){uint32_t u;memcpy(&u,&f,4);return u;}
static float scalar(uint32_t u){float f;memcpy(&f,&u,4);return f;}
static float f32(long double v){volatile float f=(float)v;return f;}
static int finite_value(float f){return (bits(f)&0x7f800000u)!=0x7f800000u;}
static int address(uint64_t base,uint64_t offset,uint64_t*out){
 if(!base||base>UINT64_MAX-offset||base+offset>UINT64_MAX-4)return 0;
 *out=base+offset;return 1;
}
static int read_at(NativeRuntime*r,uint64_t a,uint32_t*out){
 if(!a||a>UINT64_MAX-4||!r->read||!r->read(r->context,a,out,4)){r->stats.reads_failed++;return 0;}return 1;
}
static int write_at(NativeRuntime*r,uint64_t a,const uint32_t*value){
 if(!a||a>UINT64_MAX-4||!r->write||!r->write(r->context,a,value,4)){r->stats.writes_failed++;return 0;}return 1;
}
void native_fov_reset(NativeRuntime*r){
 NativeFovState*s=&r->fov;
 s->reference=s->target=0;s->reference_bits=s->target_bits=s->area=0;
 s->runningfovadd=s->ratiorunfovmax=0;
 s->runfovcalcflag=s->active=s->have_area=0;
}
int native_runtime_set_fov_preferences(NativeRuntime*r,int custom,float degrees,
 int first_person_run,int third_person_run,float amount,float rise,float fall){
 if((custom!=0&&custom!=1)||(first_person_run!=0&&first_person_run!=1)||
    (third_person_run!=0&&third_person_run!=1)||!finite_value(degrees)||
    !finite_value(amount)||!finite_value(rise)||!finite_value(fall)||
    degrees<60||degrees>120||amount<0||amount>40||rise<.1f||rise>10||fall<.1f||fall>5)return 0;
 NativeFovState*s=&r->fov;
 if(s->customfovflag!=custom||s->custom_fov!=degrees||s->fprunfov!=first_person_run||
    s->tprunfov!=third_person_run||s->runningfovmax!=amount||
    s->fovrunincbaserate!=rise||s->fovrundecbaserate!=fall){
  native_fov_reset(r); /* A new policy must acquire fresh engine operands. */
  s->customfovflag=(uint8_t)custom;s->custom_fov=degrees;
  s->fprunfov=(uint8_t)first_person_run;s->tprunfov=(uint8_t)third_person_run;
  s->runningfovmax=amount;s->fovrunincbaserate=rise;s->fovrundecbaserate=fall;
 }
 return 1;
}
static int reference_live(NativeRuntime*r){
 uint32_t first,second;NativeFovState*s=&r->fov;
 return s->reference&&read_at(r,s->reference,&first)&&first==s->reference_bits&&
  read_at(r,s->reference,&second)&&second==first;
}
static int control_live(NativeRuntime*r){
 NativeFovState*s=&r->fov;uint32_t first,second;
 if(!s->active||s->nofovtamperflag==1||s->reframeworkfovframes<3)return 0;
 if(!reference_live(r)||!read_at(r,s->target,&first)||first!=s->target_bits||
    !read_at(r,s->target,&second)||second!=first||!reference_live(r)){native_fov_reset(r);return 0;}
 return 1;
}
static float calculate(NativeRuntime*r,NativeFovState*s){
 float menu=f32(5.L*s->menu_index);menu=f32(70.L+menu);
 float actual=s->customfovflag? s->custom_fov:menu;
 if(r->state.stopflag==1)return actual;
 if(r->inputs.state.runflag!=0)s->runfovcalcflag=1;
 if(s->runfovcalcflag!=1)s->runningfovadd=0;
 else {
  float fraction=f32((long double)s->runningfovmax/100.f);
  s->ratiorunfovmax=f32((long double)actual*fraction);
  float tick=f32(fabsl((long double)s->ratiorunfovmax/r->inputs.state.framerate/s->fovrunincbaserate));
  int increment=r->inputs.state.runflag!=0;
  if(increment&&((s->fprunfov!=1&&r->view.firstpersonflag==1)||
                 (s->tprunfov!=1&&r->view.firstpersonflag!=1))){s->runningfovadd=0;increment=0;}
  if(increment){
   if(bits(s->runningfovadd)<=bits(s->ratiorunfovmax))s->runningfovadd=f32((long double)s->runningfovadd+tick);
  }else{
   tick=f32(fabsl((long double)s->ratiorunfovmax/r->inputs.state.framerate/s->fovrundecbaserate));
   if(bits(s->runningfovadd)>=0x80000000u)s->runningfovadd=0;
   if(bits(s->runningfovadd)!=0)s->runningfovadd=f32((long double)s->runningfovadd-tick);
   else s->runfovcalcflag=0;
  }
 }
 if(r->inputs.state.flycamflag==1)return actual;
 /* CE @f resolves to the next named factorgamefovchange label here. */
 if(s->preventactionfovchange!=1&&!(r->config.zoomaimflag!=0&&r->view.firstpersonflag==0)){
  float ratio=f32((long double)scalar(s->reference_bits)/menu);
  actual=f32((long double)actual*ratio);
 }
 return f32((long double)actual+s->runningfovadd);
}
static int fov_read(NativeRuntime*r,NativeFrame*f,void*fp){
 NativeFovState*s=&r->fov;uint64_t target;
 /* This is the original one-frame gate and camera-distance/rate update.
  * It must precede even RF grace and missing-reference checks. */
 int frame=re7_original_state_frame(&r->config,&r->inputs.state,&r->state,&r->params,&r->rates);
 if(!address(f->rdi,0x38,&target)){native_fov_reset(r);return 0;}
 if(s->target&&target!=s->target)s->active=0;
 if(!frame)return 0;
 s->active=0;
 if(s->nofovtamperflag==1)return 0;
 if(++s->reframeworkfovframes>=1000)s->reframeworkfovframes=100;
 if(s->reframeworkfovframes<3)return 0;
 if(!reference_live(r)){native_fov_reset(r);return 0;}
 if((target<s->reference+4&&s->reference<target+4)||
    !finite_value(r->inputs.state.framerate)||r->inputs.state.framerate<=0||
    !finite_value(s->fovrunincbaserate)||s->fovrunincbaserate<=0||
    !finite_value(s->fovrundecbaserate)||s->fovrundecbaserate<=0)return 0;
 NativeFovState next=*s;float result=calculate(r,&next);uint32_t old,value=bits(result);
 if(!finite_value(result)||!read_at(r,target,&old)||!reference_live(r))return 0;
 if(!write_at(r,target,&value)){
  /* The attempted DWORD is the rollback ownership token. A competing
   * store may retire it while either source or destination is checked.
   * Restore only our still-current write, never another writer's value. */
  uint32_t current;
  if(reference_live(r)&&read_at(r,target,&current)&&current==value&&
     reference_live(r)&&read_at(r,target,&current)&&current==value)write_at(r,target,&old);
  native_fov_reset(r);return 0;
 }
 uint32_t confirmed;
 if(!reference_live(r)||!read_at(r,target,&confirmed)||confirmed!=value||!reference_live(r)){native_fov_reset(r);return 0;}
 next.target=target;next.target_bits=value;next.active=1;*s=next;
 native_set_xmm_scalar(fp,0,result);return 1;
}
int native_fov_dispatch(NativeRuntime*r,unsigned site,NativeFrame*f,void*fp){
 NativeFovState*s=&r->fov;uint64_t dst;uint32_t value;
 int ready=r->camera_enabled&&r->inputs.ready&&!r->inputs.state.loadscreenflag;
 if(!ready||(s->have_area&&s->area!=r->inputs.state.loadarea))native_fov_reset(r);
 if(site==NS_MENU_FOV_BYTE){
  if(!address(f->rax,0x18,&dst)||!read_at(r,dst,&value))return 0;
  if((uint32_t)s->menu_index!=value)s->active=0;
  memcpy(&s->menu_index,&value,4);
  float lanes[4]={0};memcpy(lanes,&value,4);native_set_xmm_vector(fp,0,lanes);return 1;
 }
 if(site==NS_REFRAMEWORK_FOV_WRITE_POINT){
  /* Revoke BEFORE attempting the original store, including a failed store. */
  s->active=0;s->reframeworkfovframes=0;
  value=bits(native_get_xmm_scalar(fp,2));
  return address(f->rdx,0x38,&dst)&&write_at(r,dst,&value);
 }
 if(!ready)return 0;
 if(!s->have_area){s->area=r->inputs.state.loadarea;s->have_area=1;}
 switch(site){
 case NS_TRUE_FOV_REFERENCE:
  if(!address(f->rbx,0x2a8,&dst)){native_fov_reset(r);return 0;}
  value=bits(native_get_xmm_scalar(fp,6));
  if(dst!=s->reference||value!=s->reference_bits)s->active=0;
  if(!write_at(r,dst,&value)){native_fov_reset(r);return 0;}
  s->reference=dst;s->reference_bits=value;return 1;
 case NS_TRUE_FOV_READ:return fov_read(r,f,fp);
 case NS_FOV_WRITE_MAIN:case NS_FOV_WRITE_NAH:case NS_FOV_WRITE_EOZ:
  if(!address(site==NS_FOV_WRITE_EOZ?f->rdx:f->rax,0x38,&dst)||!control_live(r))return 0;
  return dst==s->target;
 default:return 0;
 }
}
