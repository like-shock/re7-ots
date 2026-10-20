#include "native_runtime.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static unsigned char mem[1048576];
static unsigned writes,reads;static uint64_t failread,failwrite,interaction_pointer;
static int rd(void*c,uint64_t a,void*out,size_t n){(void)c;reads++;if(a==failread)return 0;if(a==0x8fc17f0&&n<=8){memcpy(out,&interaction_pointer,n);return 1;}if(a>sizeof mem||n>sizeof mem-a)return 0;memcpy(out,mem+a,n);return 1;}
static int wr(void*c,uint64_t a,const void*in,size_t n){(void)c;writes++;if(a==failwrite||a>sizeof mem||n>sizeof mem-a)return 0;memcpy(mem+a,in,n);return 1;}
static void putf(unsigned a,float f){memcpy(mem+a,&f,4);}
static float getf(unsigned a){float f;memcpy(&f,mem+a,4);return f;}
static uint32_t bits(float f){uint32_t v;memcpy(&v,&f,4);return v;}
int __attribute__((ms_abi)) native_dispatch(unsigned site,NativeFrame*f,void*fp){(void)site;(void)f;(void)fp;return 0;}
static void ready(NativeRuntime*r){native_runtime_init(r,0x10000,rd,wr,0);r->inputs.ready=1;r->inputs.valid=~0ULL;r->inputs.aim_address=0x60000;r->inputs.state.player_available=1;r->inputs.state.autooldcamfov=75;r->inputs.state.loadarea=4;r->camera_base=0x2000;r->camera_valid=1;r->yawx=0;r->yawz=1;putf(0x2000,1);putf(0x2008,0);putf(0x2024,0);putf(0x2030,10);putf(0x2034,20);putf(0x2038,30);writes=reads=0;failread=failwrite=interaction_pointer=0;}
int main(void){
 NativeRuntime r;NativeFrame f={0};unsigned char fp[1024]={0},before[1024];
 ready(&r);f.rbx=0x2100;f.rbp=0x3000;putf(0x3030,1);native_set_xmm_scalar(fp,0,10);native_set_xmm_scalar(fp,1,20);native_set_xmm_scalar(fp,2,30);native_set_xmm_scalar(fp,15,.5f);
 CHECK(!native_runtime_dispatch(&r,NS_CAM_WRITES,&f,fp));CHECK(!writes&&!reads);
 f.rbx=0x2000;memset(fp+160+4,0xab,12);memcpy(before,fp,sizeof fp);NativeFrame orig=f;
 CHECK(native_runtime_dispatch(&r,NS_CAM_WRITES,&f,fp));CHECK(r.stats.camera_matches==1);CHECK(!memcmp(&f,&orig,sizeof f));
 CHECK(getf(0x2024)==.5f);CHECK(getf(0x2030)==10.3f);CHECK(fabsf(getf(0x2034)-20.45f)<.00001f);CHECK(fabsf(getf(0x2038)-30.91f)<.00001f);CHECK(getf(0x203c)==1);
 CHECK(native_get_xmm_scalar(fp,0)==1);for(int i=4;i<16;i++)CHECK(fp[160+i]==0);CHECK(!memcmp(fp+176,before+176,512-176));
 ready(&r);f.rbx=0x2000;failread=0x3030;memcpy(before,fp,sizeof fp);CHECK(!native_runtime_dispatch(&r,NS_CAM_WRITES,&f,fp));CHECK(!writes);CHECK(!memcmp(fp,before,sizeof fp));
 ready(&r);failwrite=0x2030;CHECK(!native_runtime_dispatch(&r,NS_CAM_WRITES,&f,fp));CHECK(!memcmp(fp,before,sizeof fp));CHECK(r.stats.writes_failed==1);
 ready(&r);f.rdi=0x4000;orig=f;memcpy(before,fp,sizeof fp);CHECK(native_runtime_dispatch(&r,NS_FIRING_COOR_WRITES,&f,fp));CHECK(getf(0x4530)==10&&getf(0x4534)==20&&getf(0x4538)==30);CHECK(!memcmp(&f,&orig,sizeof f)&&!memcmp(fp,before,sizeof fp));
 putf(0x2030,77);CHECK(native_runtime_dispatch(&r,NS_FIRING_COOR_WRITES,&f,fp));CHECK(getf(0x4530)==77); /* live camera, not old cache */
 failread=0x2030;writes=0;CHECK(!native_runtime_dispatch(&r,NS_FIRING_COOR_WRITES,&f,fp)&&!writes);
 ready(&r);f.rax=0x5000;f.rbp=0x3000;putf(0x5008,99);CHECK(native_runtime_dispatch(&r,NS_EOZ_AIM_POS_WRITE,&f,fp));CHECK(f.rax==bits(99)&&f.rcx==0x35d0);CHECK(getf(0x4538)==30);
 ready(&r);r.view.firstpersonflag=1;f.rbx=0x2000;f.rbp=0x3000;native_set_xmm_scalar(fp,0,10);native_set_xmm_scalar(fp,1,20);native_set_xmm_scalar(fp,2,30);CHECK(native_runtime_dispatch(&r,NS_CAM_WRITES,&f,fp));CHECK(getf(0x2030)==10&&getf(0x2034)==20&&getf(0x2038)==30);
 ready(&r);native_runtime_shoulder(&r);CHECK(r.params.sidedis==-1);native_runtime_shoulder(&r);CHECK(r.params.sidedis==1);
 /* The UI option changes ONLY the original melee gate, not inventory/events. */
 ready(&r);r.inputs.state.loadarea=5;r.inputs.state.player=(Re7OriginalVec){100,100,100};r.inputs.state.holdingmelee=1;
 re7_original_state_policy(&r.config,&r.inputs.state,&r.state);CHECK(r.state.blockedgun==1);
 native_runtime_set_melee_auto_disable(&r,0);
 re7_original_state_policy(&r.config,&r.inputs.state,&r.state);CHECK(r.state.blockedgun==0&&r.config.fpmeleeflag==0);
 r.inputs.state.inventory=1;re7_original_state_policy(&r.config,&r.inputs.state,&r.state);CHECK(r.state.blockedgun==1);
 r.inputs.state.inventory=0;native_runtime_set_melee_auto_disable(&r,1);
 re7_original_state_policy(&r.config,&r.inputs.state,&r.state);CHECK(r.state.blockedgun==1);
 native_runtime_set_melee_auto_disable(&r,9);CHECK(r.config.fpmeleeflag==1);
 ready(&r);r.inputs.valid&=~RE7_INPUT_AREA;r.inputs.ready=0;f.rbx=0x2000;CHECK(!native_runtime_dispatch(&r,NS_CAM_WRITES,&f,fp));CHECK(!writes);
 /* Ordinary HEAD transforms publish source view policy with zero memory reads
  * when fpaim is off and no inspection latch can match. */
 ready(&r);r.state.stopflag=1;CHECK(!native_runtime_dispatch(&r,NS_HEAD_Y_WRITE,&f,fp));CHECK(r.view.firstpersonflag==1&&r.stats.view_changes==1&&!reads&&!writes);
 r.state.stopflag=0;CHECK(!native_runtime_dispatch(&r,NS_HEAD_Y_WRITE,&f,fp));CHECK(r.view.firstpersonflag==0&&r.stats.view_changes==2&&!reads&&!writes);
 /* Source secondary-object branch, all XYZ + W, correct XMM output effects. */
 ready(&r);r.inputs.state.interactableflag=1;r.inputs.state.lockboxflag=5;interaction_pointer=0x90000;r.interaction_input.secintadd=0x123456;f.r13=0x123456;f.rbx=0x8000;f.rbp=0x3000;
 r.params.pitchmul=1;native_set_xmm_scalar(fp,0,10);native_set_xmm_scalar(fp,1,20);native_set_xmm_scalar(fp,2,30);memset(fp+164,0x77,12);orig=f;memcpy(before,fp,sizeof fp);
 CHECK(native_runtime_dispatch(&r,NS_TP_INTERACTABLE_COORDS,&f,fp));CHECK(getf(0x8030)==10.3f&&getf(0x8034)==20&&getf(0x8038)==31.3f&&getf(0x803c)==1);CHECK(native_get_xmm_scalar(fp,0)==1);CHECK(!memcmp(&f,&orig,sizeof f));CHECK(!memcmp(fp+176,before+176,512-176));
 native_set_xmm_scalar(fp,9,10);native_set_xmm_scalar(fp,0,20);native_set_xmm_scalar(fp,1,30);native_set_xmm_scalar(fp,2,7);f.r15=2;memcpy(before,fp,sizeof fp);
 CHECK(native_runtime_dispatch(&r,NS_HEAD_Y_WRITE,&f,fp));CHECK(getf(0x8030)==10.3f&&getf(0x8034)==20&&getf(0x8038)==31.3f&&getf(0x803c)==7);CHECK(!memcmp(fp,before,sizeof fp));
 /* Original transparency byte plus exact EAX zero-extension, then gun store suppression. */
 ready(&r);uint64_t owner=0x70000;memcpy(mem+0x3010,&owner,8);uint32_t original=0x11223344;memcpy(mem+0x703d0,&original,4);f.rbp=0x3000;f.rax=UINT64_MAX;
 CHECK(native_runtime_dispatch(&r,NS_PLAYER_TRANS_READ,&f,fp));CHECK(mem[0x703d1]==6&&f.rax==0x11220644);
 f.rcx=owner;orig=f;writes=0;CHECK(native_runtime_dispatch(&r,NS_TRANS_GUN_WRITE,&f,fp));CHECK(!writes&&!memcmp(&f,&orig,sizeof f));interaction_pointer=1;CHECK(!native_runtime_dispatch(&r,NS_TRANS_GUN_WRITE,&f,fp));
 /* Both DOF conventions retain exact replaced-block outputs. Numeric source
  * agreement is independently checked against the CT by collision tests. */
 ready(&r);r.params.pitchmul=1;r.collision.sample_valid=1;r.collision.dofattempt=10;r.collision.rawdofattempt=10;r.camera_attempt=(Re7OriginalVec){10,20,30};
 f.rdi=0x4000;f.rax=0x5000;f.rcx=bits(.1f);putf(0x5004,.2f);putf(0x5008,1);memcpy(before,fp,sizeof fp);
 CHECK(native_runtime_dispatch(&r,NS_DOF_ORIGIN_ANGLE_WRITES,&f,fp));CHECK(f.rcx==bits(getf(0x4564))&&f.rax==bits(getf(0x4568)));CHECK(!memcmp(fp,before,sizeof fp));
 native_set_xmm_scalar(fp,0,.1f);native_set_xmm_scalar(fp,1,.2f);native_set_xmm_scalar(fp,2,1);for(int j=0;j<3;j++)memset(fp+164+16*j,0xab,12);orig=f;
 CHECK(native_runtime_dispatch(&r,NS_NAH_DOF_ORIGIN_ANGLE_WRITES,&f,fp));CHECK(!memcmp(&f,&orig,sizeof f));for(int j=0;j<3;j++){CHECK(native_get_xmm_scalar(fp,j)==getf(0x4560+4*j));for(int k=4;k<16;k++)CHECK(fp[160+16*j+k]==0);}
 /* An examine capture cannot modify XMM7 or the instruction's source value. */
 native_set_xmm_scalar(fp,7,2);r.collision.rawdofattempt=.25f;memcpy(before,fp,sizeof fp);orig=f;CHECK(!native_runtime_dispatch(&r,NS_EXAMINE_DOF_WRITE,&f,fp));CHECK(r.collision.dofattempt==2);CHECK(!memcmp(&f,&orig,sizeof f)&&!memcmp(fp,before,sizeof fp));
 /* Final-boss watch exclusion is defined in RAW attempted coordinates (CT
  * 7813-7821), not the displaced render pose. This was missed by core-only
  * oracle tests; drive the actual camera adapter before the source policy. */
 ready(&r);r.inputs.state.loadarea=6;r.inputs.state.player=(Re7OriginalVec){-107,0,-127};r.inputs.state.autooldcamfov=73;r.inputs.state.igtimer=22;r.state.finaligtimer=10;r.params.pitchmul=1;
 putf(0x2000,0);putf(0x2008,1);f.rbx=0x2000;f.rbp=0x3000;native_set_xmm_scalar(fp,0,-107.2f);native_set_xmm_scalar(fp,1,-1);native_set_xmm_scalar(fp,2,-128.5f);native_set_xmm_scalar(fp,15,0);
 CHECK(native_runtime_dispatch(&r,NS_CAM_WRITES,&f,fp));CHECK(fabsf(getf(0x2030)- -108.5f)<.00001f);
 CHECK(r.inputs.state.camera.x==-107.2f&&r.inputs.state.camera.y==-1&&r.inputs.state.camera.z==-128.5f);
 NativeSnapshot snap;native_runtime_snapshot(&r,&snap);CHECK(snap.camera[0]==getf(0x2030));
 re7_original_state_policy(&r.config,&r.inputs.state,&r.state);CHECK(r.state.blockedgun==1&&r.state.fakegbflag==1&&r.state.finaligtimepassed==0);
 f.rdi=0x20a0;CHECK(!native_runtime_dispatch(&r,NS_CAM_AFTER_1000_READ,&f,fp));CHECK(r.inputs.state.camera.x==-107.2f); /* observation cannot overwrite raw input */
 /* Partial and exact source-W aliases cannot be emulated from a pre-read.
  * Unsupported aliases must replay the original block without any writes. */
 for(int i=0;i<3;i++){
  ready(&r);f.rbx=0x2000;f.rbp=(uint64_t[]){0x2000,0x2001,0x1ff4}[i];memcpy(before,fp,sizeof fp);orig=f;
  CHECK(!native_runtime_dispatch(&r,NS_CAM_WRITES,&f,fp));CHECK(!writes&&!memcmp(fp,before,sizeof fp)&&!memcmp(&f,&orig,sizeof f));
 }
 ready(&r);f.rdi=0x4000;f.rax=0x4528;orig=f;CHECK(!native_runtime_dispatch(&r,NS_EOZ_AIM_POS_WRITE,&f,fp));CHECK(!writes&&!memcmp(&f,&orig,sizeof f));
 puts("native runtime contract passed");return 0;
}
