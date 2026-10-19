/* CT2474-2853,3764-3777,4151-4188: catch wrong source arithmetic,
 * cadence, full instruction effects, stale suppression and >64 masks. */
#include "native_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"line %d: %s\n",__LINE__,#c);exit(1);}}while(0)
#define M UINT64_C(0x100000)
#define BASE UINT64_C(0x140000000)
#define MENU (M+0x1018)
#define REF (M+0x22a8)
#define TARGET (M+0x3038)
static unsigned char memory[0x10000],fp[1024],saved_fp[1024];
static NativeRuntime r;static NativeFrame f,saved;
static uint64_t bad_read,replace_at,replace_address;static uint32_t replace_value;
static unsigned writes,fail_write;static int partial;
static void put(uint64_t a,const void*v,size_t n){CHECK(a>=M&&a-M+n<=sizeof memory);memcpy(memory+a-M,v,n);}
static void dword(uint64_t a,uint32_t v){put(a,&v,4);}
static uint32_t get(uint64_t a){uint32_t v;memcpy(&v,memory+a-M,4);return v;}
static uint32_t bits(float v){uint32_t u;memcpy(&u,&v,4);return u;}
static int rd(void*c,uint64_t a,void*out,size_t n){
 (void)c;if(a==bad_read||a<M||a-M>=sizeof memory||n>sizeof memory-(a-M))return 0;
 memcpy(out,memory+a-M,n);
 if(a==replace_at){dword(replace_address,replace_value);replace_at=UINT64_MAX;}
 return 1;
}
static int wr(void*c,uint64_t a,const void*in,size_t n){
 (void)c;writes++;if(a<M||a-M>=sizeof memory||n>sizeof memory-(a-M))return 0;
 if(writes==fail_write){if(partial)memcpy(memory+a-M,in,n);return 0;}
 memcpy(memory+a-M,in,n);return 1;
}
int __attribute__((ms_abi)) native_dispatch(unsigned s,NativeFrame*g,void*p){(void)s;(void)g;(void)p;return 0;}
static unsigned site(const char*name){for(unsigned j=0;j<NATIVE_SITE_COUNT;j++)if(!strcmp(native_sites[j].name,name))return j;return NATIVE_SITE_COUNT;}
static int call(const char*name){return native_runtime_dispatch(&r,site(name),&f,fp);}
static void save(void){saved=f;memcpy(saved_fp,fp,sizeof fp);writes=0;}
static void unchanged(void){CHECK(!memcmp(&f,&saved,sizeof f));CHECK(!memcmp(fp,saved_fp,sizeof fp));}
static void ready(void){r.inputs.valid=RE7_INPUT_REQUIRED;r.inputs.state.player_available=1;r.inputs.ready=1;r.inputs.state.loadscreenflag=0;}
static void setup(void){
 memset(memory,0,sizeof memory);memset(fp,0x53,sizeof fp);memset(&f,0x72,sizeof f);
 native_runtime_init(&r,BASE,rd,wr,0);ready();r.inputs.state.loadarea=35;
 bad_read=replace_at=UINT64_MAX;writes=fail_write=partial=0;
 native_use_xsave=1;fp[512]&=~2;f.flags=0xed7;
 dword(MENU,4);dword(REF,bits(72));dword(TARGET,bits(123));
 f.rdi=TARGET-0x38;f.rax=MENU-0x18;f.rbx=REF-0x2a8;
}
static void producers(void){
 f.rax=MENU-0x18;CHECK(call("MENU_FOV_BYTE"));
 f.rbx=REF-0x2a8;float value=72;memcpy(fp+256,&value,4);CHECK(call("TRUE_FOV_REFERENCE"));
}
static int tick(void){r.inputs.frametrack++;f.rdi=TARGET-0x38;return call("TRUE_FOV_READ");}
static int suppress(const char*name,uint64_t target){f.rax=f.rdx=target-0x38;save();return call(name);}
static void menu(void){
 setup();save();CHECK(call("MENU_FOV_BYTE"));uint32_t want[]={4,0,0,0};
 CHECK(!memcmp(fp+160,want,16));CHECK(fp[512]&2);memcpy(saved_fp+160,want,16);saved_fp[512]|=2;unchanged();CHECK(!writes);
 setup();bad_read=MENU;save();CHECK(!call("MENU_FOV_BYTE"));unchanged();CHECK(!writes);
}
static void reference(void){
 setup();float v=81;memcpy(fp+256,&v,4);save();CHECK(call("TRUE_FOV_REFERENCE"));unchanged();CHECK(get(REF)==bits(81)&&writes==1);
 setup();r.inputs.state.loadscreenflag=1;save();CHECK(!call("TRUE_FOV_REFERENCE"));unchanged();CHECK(!writes);
}
static void ratio(void){
 setup();producers();save();CHECK(tick());CHECK(get(TARGET)==bits(90));
 float want[]={90,0,0,0};memcpy(saved_fp+160,want,16);saved_fp[512]|=2;unchanged();CHECK(writes==1);
 r.view.firstpersonflag=1;save();CHECK(tick());CHECK(get(TARGET)==bits(72));
 r.state.stopflag=1;save();CHECK(tick());CHECK(get(TARGET)==bits(90));
}
static void cadence(void){
 setup();producers();r.config.runningfov=1;r.params.backdisadjust=1;r.inputs.state.runflag=1;CHECK(tick());uint32_t first=get(TARGET);CHECK(first==0x42b415f1u);
 float back=r.params.backdisadjust;save();CHECK(!call("TRUE_FOV_READ"));unchanged();CHECK(!writes&&get(TARGET)==first&&r.params.backdisadjust==back);
 CHECK(tick());CHECK(get(TARGET)==0x42b42be3u&&r.params.backdisadjust>back);
}
static void run(void){
 setup();producers();r.inputs.state.runflag=1;CHECK(tick());CHECK(get(TARGET)==0x42b415f1u);
 r.inputs.state.runflag=0;CHECK(tick());CHECK(get(TARGET)==0x42b33a84u); /* one negative decay frame */
 CHECK(tick());CHECK(get(TARGET)==bits(90));
 setup();producers();r.view.firstpersonflag=1;r.inputs.state.runflag=1;CHECK(tick());CHECK(get(TARGET)==bits(72));
}
static void overshoot(void){
 setup();producers();r.inputs.state.framerate=1;r.inputs.state.runflag=1;
 for(unsigned j=0;j<8;j++){
  CHECK(tick());
 }
 CHECK(get(TARGET)==0x42dd2492u); /* 90 + 8*(18/7) */
 CHECK(tick());CHECK(get(TARGET)==0x42dd2492u); /* unsigned >max, no clamp */
}
static void authority(void){
 const char*sites[]={"FOV_WRITE_MAIN","FOV_WRITE_NAH","FOV_WRITE_EOZ"};
 for(unsigned j=0;j<3;j++){
  setup();save();CHECK(!suppress(sites[j],TARGET));unchanged();CHECK(!writes);
  producers();CHECK(tick());save();CHECK(suppress(sites[j],TARGET));unchanged();CHECK(!writes);
  CHECK(!suppress(sites[j],TARGET+0x100));unchanged();CHECK(!writes);
  dword(TARGET,bits(91));CHECK(!suppress(sites[j],TARGET));CHECK(!writes);
 }
}
static void rf(void){
 setup();producers();CHECK(tick());f.rdx=TARGET-0x38;float v=111;memcpy(fp+192,&v,4);save();
 CHECK(call("REFRAMEWORK_FOV_WRITE_POINT"));unchanged();CHECK(get(TARGET)==bits(111)&&writes==1);
 save();CHECK(!suppress("FOV_WRITE_MAIN",TARGET));unchanged();CHECK(!writes);
 CHECK(!tick());CHECK(!tick());CHECK(tick());CHECK(get(TARGET)==bits(90));
 setup();producers();CHECK(tick());f.rdx=TARGET-0x38;save();fail_write=1;CHECK(!call("REFRAMEWORK_FOV_WRITE_POINT"));unchanged();
 fail_write=0;save();CHECK(!suppress("FOV_WRITE_MAIN",TARGET));CHECK(!writes);
}
static void missing(void){
 setup();save();CHECK(!tick());unchanged();CHECK(!writes);
 setup();producers();CHECK(tick());bad_read=REF;save();CHECK(!suppress("FOV_WRITE_MAIN",TARGET));unchanged();CHECK(!writes);
 bad_read=UINT64_MAX;save();CHECK(!tick());unchanged();CHECK(!writes);producers();CHECK(tick());
 setup();producers();CHECK(tick());bad_read=MENU;save();CHECK(tick());CHECK(get(TARGET)==bits(90));
 setup();f.rbx=REF-0x2a8;float v=72;memcpy(fp+256,&v,4);CHECK(call("TRUE_FOV_REFERENCE"));CHECK(tick());CHECK(get(TARGET)==bits(70));
}
static void changed(void){
 setup();producers();CHECK(tick());f.rbx=REF+0x100-0x2a8;CHECK(call("TRUE_FOV_REFERENCE"));save();CHECK(!suppress("FOV_WRITE_MAIN",TARGET));unchanged();CHECK(!writes);
 setup();producers();CHECK(tick());f.rax=MENU+0x100-0x18;dword(MENU+0x100,6);CHECK(call("MENU_FOV_BYTE"));save();CHECK(!suppress("FOV_WRITE_MAIN",TARGET));unchanged();CHECK(!writes);
 setup();producers();CHECK(tick());f.rdi=TARGET+0x100-0x38;save();CHECK(!call("TRUE_FOV_READ"));unchanged();CHECK(!writes);CHECK(!suppress("FOV_WRITE_MAIN",TARGET));
}
static void race(void){
 setup();producers();CHECK(tick());replace_at=REF;replace_address=REF;replace_value=bits(60);save();
 CHECK(!suppress("FOV_WRITE_MAIN",TARGET));unchanged();CHECK(!writes);
 setup();producers();CHECK(tick());replace_at=TARGET;replace_address=REF;replace_value=bits(60);save();
 CHECK(!suppress("FOV_WRITE_MAIN",TARGET));unchanged();CHECK(!writes);
 setup();producers();replace_at=REF;replace_address=REF;replace_value=bits(60);save();CHECK(!tick());unchanged();CHECK(!writes);
}
static void failures(void){
 setup();producers();save();fail_write=1;partial=1;CHECK(!tick());unchanged();
 fail_write=0;save();CHECK(!suppress("FOV_WRITE_MAIN",TARGET));unchanged();CHECK(!writes);
 setup();producers();f.rdi=UINT64_MAX-0x20;r.params.heightdisadjust=33;save();r.inputs.frametrack++;CHECK(!call("TRUE_FOV_READ"));unchanged();CHECK(!writes);CHECK(r.params.heightdisadjust==.9f);
}
static void reset(void){
 for(unsigned j=0;j<4;j++){
  setup();producers();CHECK(tick());
  if(j==0){native_runtime_set_camera_enabled(&r,0);save();CHECK(!suppress("FOV_WRITE_MAIN",TARGET));unchanged();CHECK(!writes);native_runtime_set_camera_enabled(&r,1);r.view.firstpersonflag=0;}
  if(j==1){r.inputs.state.loadscreenflag=1;CHECK(!call("TRUE_FOV_READ"));ready();}
  if(j==2){r.inputs.valid&=~RE7_INPUT_PLAYER;CHECK(!call("TRUE_FOV_READ"));ready();}
  if(j==3){r.inputs.state.loadarea=34;CHECK(!call("TRUE_FOV_READ"));r.inputs.state.loadarea=35;}
  save();CHECK(!tick());unchanged();CHECK(!writes);producers();CHECK(tick());
 }
}
static void counts(void){
 setup();CHECK(NATIVE_SITE_COUNT==67);NativeSnapshot snap;
 const unsigned indexes[]={64,66};
 for(unsigned j=0;j<2;j++){unsigned s=indexes[j];save();CHECK(!native_runtime_dispatch(&r,s,&f,fp));unchanged();CHECK(r.inputs.producer_calls[s]==1);native_runtime_snapshot(&r,&snap);CHECK(snap.calls[s]==1);CHECK(snap.input_observed_high&(UINT64_C(1)<<(s-64)));}
 CHECK(r.inputs.observed==0); /* high sites must never wrap into low word */
 native_runtime_set_camera_enabled(&r,0);
 for(unsigned j=0;j<2;j++){unsigned s=indexes[j];save();CHECK(!native_runtime_dispatch(&r,s,&f,fp));unchanged();CHECK(!writes&&r.inputs.producer_calls[s]==1);}
}

/* A failed write may have copied its full DWORD before reporting failure.
 * Another writer can then retire it while the source reference is checked. */
static int retire_target_after_failed_write(void*c,uint64_t a,const void*in,size_t n){
 int ok=wr(c,a,in,n);
 if(!ok&&a==TARGET){replace_at=REF;replace_address=TARGET;replace_value=bits(333);}
 return ok;
}
static void rollback_owner(void){
 setup();producers();r.write=retire_target_after_failed_write;save();fail_write=1;partial=1;
 CHECK(!tick());unchanged();CHECK(get(TARGET)==bits(333));CHECK(writes==1);
 /* Conversely, the failed write still belongs to us if its attempted bits
  * remain unchanged: restoration is then required before original MOVSS. */
 setup();producers();save();fail_write=1;partial=1;CHECK(!tick());unchanged();
 CHECK(get(TARGET)==bits(123));CHECK(writes==2);
 setup();producers();save();fail_write=1;CHECK(!tick());unchanged();
 CHECK(get(TARGET)==bits(123));CHECK(writes==1);
}
static void rf_readiness(void){
 for(unsigned lost=0;lost<3;lost++){
  setup();producers();CHECK(tick());f.rdx=TARGET-0x38;float value=111;memcpy(fp+192,&value,4);
  CHECK(call("REFRAMEWORK_FOV_WRITE_POINT"));
  if(lost==0)r.inputs.valid&=~RE7_INPUT_PLAYER;
  if(lost==1)r.inputs.state.loadscreenflag=1;
  if(lost==2)r.inputs.state.loadarea=34;
  CHECK(!suppress("FOV_WRITE_MAIN",TARGET));ready();r.inputs.state.loadarea=35;
  f.rbx=REF-0x2a8;value=72;memcpy(fp+256,&value,4);CHECK(call("TRUE_FOV_REFERENCE"));
  for(unsigned frame=0;frame<2;frame++){
   save();CHECK(!tick());unchanged();CHECK(!writes&&get(TARGET)==bits(111));
   save();CHECK(!call("TRUE_FOV_READ"));unchanged();CHECK(!writes);
  }
  CHECK(tick());CHECK(get(TARGET)==bits(90));
 }
}

int main(int argc,char**argv){CHECK(argc==2);
#define CASE(n) if(!strcmp(argv[1],#n)){n();puts("FOV " #n ": PASS");return 0;}
 CASE(menu) CASE(reference) CASE(ratio) CASE(cadence) CASE(run) CASE(overshoot) CASE(authority)
 CASE(rf) CASE(missing) CASE(changed) CASE(race) CASE(failures) CASE(reset) CASE(counts) CASE(rollback_owner) CASE(rf_readiness)
 return 2;
}
