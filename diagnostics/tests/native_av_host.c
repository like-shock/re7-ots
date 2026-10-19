#include "native_runtime.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"line %d: %s\n",__LINE__,#c);exit(1);}}while(0)
static unsigned char memory[0x10000],fp[1024],saved_fp[1024];
static NativeRuntime r;static NativeFrame f,saved;
static uint64_t bad_read;static int bad_write,reads,writes;
static int rd(void*c,uint64_t a,void*out,size_t n){
 (void)c;reads++;if(a==bad_read||a>=sizeof memory||n>sizeof memory-a)return 0;
 memcpy(out,memory+a,n);return 1;
}
static int wr(void*c,uint64_t a,const void*in,size_t n){
 (void)c;writes++;if(bad_write||a>=sizeof memory||n>sizeof memory-a)return 0;
 memcpy(memory+a,in,n);return 1;
}
int __attribute__((ms_abi)) native_dispatch(unsigned s,NativeFrame*g,void*p){(void)s;(void)g;(void)p;return 0;}
static void put(uint64_t a,uint32_t v){memcpy(memory+a,&v,4);}
static uint32_t word(uint64_t a){uint32_t v;memcpy(&v,memory+a,4);return v;}
static void number(uint64_t a,float v){memcpy(memory+a,&v,4);}
static float value(uint64_t a){float v;memcpy(&v,memory+a,4);return v;}
static void lanes(unsigned n,float a,float b,float c,float d){float v[]={a,b,c,d};memcpy(fp+160+16*n,v,16);}
static unsigned site(const char*name){
 for(unsigned i=0;i<NATIVE_SITE_COUNT;i++)if(!strcmp(native_sites[i].name,name))return i;
 fprintf(stderr,"missing original hook: %s\n",name);exit(1);
}
static int call(unsigned s){return native_runtime_dispatch(&r,s,&f,fp);}
static void unchanged(void){CHECK(!memcmp(&f,&saved,sizeof f)&&!memcmp(fp,saved_fp,sizeof fp));}
static void save(void){saved=f;memcpy(saved_fp,fp,sizeof fp);}
static void setup(void){
 memset(memory,0xa5,sizeof memory);memset(fp,0,sizeof fp);memset(&f,0,sizeof f);
 bad_read=UINT64_MAX;bad_write=reads=writes=0;native_use_xsave=1;
 native_runtime_init(&r,0x140000000,rd,wr,0);
 r.inputs.valid=RE7_INPUT_REQUIRED|RE7_INPUT_FLASHLIGHT|RE7_INPUT_CHARACTER;r.inputs.state.player_available=1;
 r.inputs.ready=1;r.camera_valid=1;r.camera_base=0x1000;
 r.camera_attempt=(Re7OriginalVec){10,20,30};r.params.pitchmul=1;
 number(0x1000,.8f);number(0x1008,.6f);number(0x1024,.1f);
 r.inputs.flashlight_address=0x5000;r.inputs.observed|=1ULL<<NS_FLASHLIGHT_WRITE;
 r.inputs.state.loadarea=4;r.inputs.pcharcflag=3;
 f.flags=0x202;
}
static void fallback_guards(unsigned s){
 setup();native_runtime_set_camera_enabled(&r,0);save();CHECK(!call(s));unchanged();CHECK(!reads&&!writes);
 setup();r.inputs.state.loadscreenflag=1;save();CHECK(!call(s));unchanged();CHECK(!writes);
 setup();r.inputs.valid&=~RE7_INPUT_PLAYER;save();CHECK(!call(s));unchanged();CHECK(!writes);
}
static void steady(void){
 unsigned s=site("FLASHLIGHT_STEADY_XMM");setup();lanes(2,9,8,7,6);save();
 CHECK(call(s));float want[]={1,1,1,1};CHECK(!memcmp(fp+192,want,16));
 CHECK(!memcmp(&f,&saved,sizeof f)&&!writes);CHECK(fp[512]&2);
 setup();r.view.firstpersonflag=1;save();CHECK(!call(s));unchanged();fallback_guards(s);
}
static void position(void){
 unsigned s=site("FLASHLIGHT_ALT_WRITE");setup();f.rsi=0x4000;number(f.rsi-0x40,.9098039269f);
 lanes(8,2,3,4,5);f.rax=0x123456789;save();
 r.params.camclipbehinddis=.01f;r.params.camclipsidedis=.02f; /* not light multipliers */
 CHECK(call(s));CHECK(fabsf(value(0x4000)-10.396f)<.000002f);
 CHECK(value(0x4004)==19.8f&&fabsf(value(0x4008)-29.972f)<.000002f);
 CHECK(!memcmp(fp+160,saved_fp+160+16*8,16)&&!memcmp(&f,&saved,sizeof f));
 CHECK(word(0x3ffc)==0xa5a5a5a5&&word(0x400c)==0xa5a5a5a5);
 number(0x3fc0,.1f);CHECK(call(s)); /* source cache survives changing color */
 f.rsi+=0x100;CHECK(!call(s));f.rsi-=0x100;
 r.view.firstpersonflag=1;save();CHECK(!call(s));unchanged();
 r.state.gbunzoomingprocessflag=1;CHECK(call(s));r.state.gbunzoomingprocessflag=0;
 r.state.readjustxyzfp=2;CHECK(call(s)); /* first two transition ticks */
 NativeFrame light=f;
 CHECK(!call(NS_TIMESCALE_MULTIPLIER));CHECK(r.av.transition_frames==1);
 /* This tick's intentionally unavailable scene inputs must be reacquired. */
 r.inputs.failed=0;r.inputs.valid=RE7_INPUT_REQUIRED|RE7_INPUT_CHARACTER|RE7_INPUT_FLASHLIGHT;
 r.inputs.state.player_available=1;f=light;CHECK(call(s));
 CHECK(!call(NS_TIMESCALE_MULTIPLIER));CHECK(r.av.transition_frames==2);
 r.inputs.failed=0;r.inputs.valid=RE7_INPUT_REQUIRED|RE7_INPUT_CHARACTER|RE7_INPUT_FLASHLIGHT;
 r.inputs.state.player_available=1;f=light;save();CHECK(!call(s));unchanged();
 r.state.gbunzoomingprocessflag=1;CHECK(call(s));r.state.gbunzoomingprocessflag=0;
 for(int i=0;i<8;i++)native_av_tick(&r);CHECK(r.av.transition_frames==5);
 r.state.readjustxyzfp=0;native_av_tick(&r);CHECK(r.av.transition_frames==0);
 r.view.firstpersonflag=0;
 bad_write=1;save();CHECK(!call(s));unchanged();bad_write=0;
 bad_read=0x1000;save();CHECK(!call(s));unchanged();fallback_guards(s);
}
static void color(void){
 unsigned s=site("FLASHLIGHT_COLORS_FLOAT_READS");setup();f.rax=0x5000-0x4d0;
 number(f.rax,1);number(f.rax+4,.5f);number(f.rax+8,0);
 lanes(0,3,11,12,13);lanes(1,20,21,22,23);lanes(2,4,31,32,33);lanes(11,9,9,9,9);save();
 CHECK(call(s));CHECK(native_get_xmm_scalar(fp,0)==6);
 /* float(1/255) makes .5/step just below 127.5: source FISTP gives 127. */
 CHECK(fabsf(native_get_xmm_scalar(fp,1)-3.98431396484375f)<.000001f);
 CHECK(native_get_xmm_scalar(fp,2)==0);float zero[4]={0};CHECK(!memcmp(fp+160+176,zero,16));
 CHECK(!memcmp(fp+164,saved_fp+164,12));
 CHECK(!memcmp(fp+180,saved_fp+196,12)&&!memcmp(fp+196,saved_fp+196,12));
 CHECK(!memcmp(&f,&saved,sizeof f)&&!writes);
 r.view.firstpersonflag=1;lanes(0,3,11,12,13);lanes(2,4,31,32,33);CHECK(call(s));CHECK(native_get_xmm_scalar(fp,0)==3);
 f.rax++;save();CHECK(!call(s));unchanged();f.rax--;
 bad_read=f.rax;save();CHECK(!call(s));unchanged();bad_read=UINT64_MAX;
 r.inputs.observed&=~(1ULL<<NS_FLASHLIGHT_WRITE);save();CHECK(!call(s));unchanged();fallback_guards(s);
}
static void beam(void){
 unsigned s=site("FLASH_BEAM_WIDTH");setup();f.rdi=0x5000-0x464-0xac;save();
 CHECK(call(s));CHECK(value(f.rdi+0xac)==10);unchanged();
 f.rdi++;CHECK(!call(s));f.rdi--;r.view.firstpersonflag=1;CHECK(!call(s));r.view.firstpersonflag=0;
 r.inputs.observed&=~(1ULL<<NS_FLASHLIGHT_WRITE);CHECK(!call(s));fallback_guards(s);
}
static void audio(void){
 unsigned s=site("CAM_SOUND_COORDS_WRITE");setup();f.rsi=0x3000;f.rbx=2;f.rax=0x2000;f.rcx=0xabcdef;
 number(0x2004,123);number(0x2008,456);save();
 CHECK(call(s));CHECK(value(0x3010)==10&&value(0x3014)==20&&value(0x3018)==30);
 CHECK(f.rcx==word(0x2004)&&f.rax==word(0x2008));saved.rcx=f.rcx;saved.rax=f.rax;unchanged();
 r.view.firstpersonflag=1;f.rax=0x2000;CHECK(call(s)); /* original also covers FP */
 f.rax=0x3010;save();CHECK(!call(s));unchanged(); /* alias */
 f.rax=0x2000;f.rbx=UINT64_MAX;save();CHECK(!call(s));unchanged();
 f.rbx=2;bad_write=1;save();CHECK(!call(s));unchanged();bad_write=0;
 r.camera_valid=0;CHECK(!call(s));fallback_guards(s);
}
static void wobble(void){
 unsigned s=site("HEAD_BOB_FLAG_CMP");setup();f.flags=0xed7;save();CHECK(call(s));
 CHECK(native_handled_rva(s)==0x17ae319);
 CHECK(f.flags==((saved.flags&~0x8d5ULL)|0x44));saved.flags=f.flags;unchanged();
 r.view.firstpersonflag=1;save();CHECK(!call(s));unchanged();fallback_guards(s);
}
static void blur(void){
 unsigned s=site("DOF_CUTSCENE_BLUR_READ");setup();lanes(9,1,2,3,4);save();CHECK(call(s));
 float zero[4]={0};CHECK(!memcmp(fp+160+144,zero,16)&&!writes&&fp[512]&2);
 r.inputs.state.loadarea=3;save();CHECK(!call(s));unchanged();
 r.inputs.state.loadarea=4;r.view.firstpersonflag=1;CHECK(!call(s));fallback_guards(s);
}
static void vhs(void){
 unsigned s=site("CAM_BOXES_WRITE");setup();f.r15=0x3000;put(f.r15+0x168,0x3f2aaaab);save();
 CHECK(call(s));CHECK(value(0x311c)==-1000&&word(0x3118)==0xa5a5a5a5&&word(0x3120)==0xa5a5a5a5&&word(0x3124)==0xa5a5a5a5);unchanged();
 r.inputs.pcharcflag=4;CHECK(call(s));r.inputs.pcharcflag=5;CHECK(!call(s));r.inputs.pcharcflag=3;
 r.view.firstpersonflag=1;CHECK(!call(s));r.state.spiderpuzfpvhsexc=1;r.inputs.state.loadarea=10;CHECK(call(s));
 r.inputs.state.loadarea=9;CHECK(!call(s));r.view.firstpersonflag=0;
 put(0x3168,0);number(0x3128,1);put(0x310c,0);put(0x30f0,0);CHECK(!call(s));
 put(0x30f0,1);CHECK(call(s));put(0x30f0,0);put(0x310c,1);CHECK(call(s));
 number(0x3128,0);CHECK(!call(s));number(0x3128,1);bad_read=0x310c;CHECK(!call(s));
 bad_read=UINT64_MAX;bad_write=1;save();CHECK(!call(s));unchanged();fallback_guards(s);
}
int main(int argc,char**argv){
 CHECK(argc==2);
 if(!strcmp(argv[1],"steady"))steady();else if(!strcmp(argv[1],"position"))position();
 else if(!strcmp(argv[1],"color"))color();else if(!strcmp(argv[1],"beam"))beam();
 else if(!strcmp(argv[1],"audio"))audio();else if(!strcmp(argv[1],"wobble"))wobble();
 else if(!strcmp(argv[1],"blur"))blur();else if(!strcmp(argv[1],"vhs"))vhs();else return 2;
 puts("native audiovisual contract passed");return 0;
}
