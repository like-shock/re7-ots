/* Producer-driven lifecycle regression: readiness may recover before TIMESCALE. */
#define main optional_main
#include "native_optional_host.c"
#undef main
#define CAMERA (M+0x1000)
#define FLASH (M+0x5000)
static void camera(float x){
 u32(CAMERA,bits(.8f));u32(CAMERA+8,bits(.6f));u32(CAMERA+0x24,bits(.1f));
 f.rdi=CAMERA+0xa0;CHECK(!call(NS_CAM_AFTER_1000_READ));
 f.rbx=CAMERA;f.rbp=M+0x8000;u32(f.rbp+0x30,bits(1));
 native_set_xmm_scalar(fp,0,x);native_set_xmm_scalar(fp,1,20);
 native_set_xmm_scalar(fp,2,30);native_set_xmm_scalar(fp,15,.1f);
 CHECK(call(NS_CAM_WRITES));CHECK(r.camera_valid&&r.camera_attempt.x==x);
}
static void flashlight(uint64_t address){f.rcx=address-0x60;f.rbp=1;CHECK(!call(NS_FLASHLIGHT_WRITE));}
static void check_path(unsigned path,uint64_t address,int expected){
 const unsigned sites[]={NS_FLASHLIGHT_ALT_WRITE,NS_CAM_SOUND_COORDS_WRITE,
  NS_FLASH_BEAM_WIDTH,NS_FLASHLIGHT_COLORS_FLOAT_READS,NS_FLASHLIGHT_SHADOWS_Z_WRITE};
 if(path==0){f.rsi=M+0x2000;}
 if(path==1){f.rsi=M+0x3000;f.rbx=0;f.rax=M+0x4000;}
 if(path==2){f.rdi=address-0x464-0xac;}
 if(path==3){
  f.rax=address-0x4d0;u32(f.rax,bits(1));u32(f.rax+4,bits(1));u32(f.rax+8,bits(1));
  native_set_xmm_scalar(fp,0,3);native_set_xmm_scalar(fp,2,4);
 }
 if(path==4){f.rdi=address-0x448-0xc8;}
 unsigned char original[sizeof mem];memcpy(original,mem,sizeof mem);save();
 CHECK(call(sites[path])==expected);
 if(!expected){same();CHECK(!writes&&!memcmp(original,mem,sizeof mem));}
 else if(path==1){CHECK(get(M+0x3000)==bits(r.camera_attempt.x));}
 else if(path==2){CHECK(get(address-0x464)==bits(10));}
 else if(path==3){CHECK(native_get_xmm_scalar(fp,0)==6);}
 else if(path==4){CHECK(get(address-0x448)==9999);}
}
static void initial(void){
 setup();camera(10);flashlight(FLASH);u32(M+0x1fc0,bits(.9098039269f));
 check_path(0,FLASH,1);u32(M+0x1fc0,0);check_path(0,FLASH,1);
}
static void lost(unsigned path,int loading){
 initial();check_path(path,FLASH,1);
 if(loading){CHECK(!call(NS_TIMER_AOB));CHECK(r.inputs.state.loadscreenflag==1);}
 else{f.rax=M+0x7000;bad=f.rax+0x54;CHECK(!call(NS_LOADED_AREA));CHECK(r.inputs.failed&RE7_INPUT_AREA);}
 CHECK(!r.inputs.ready);check_path(path,FLASH,0);
 /* Samples during lost readiness cannot reauthorize a future effect. */
 flashlight(FLASH);f.rdi=CAMERA+0xa0;CHECK(!call(NS_CAM_AFTER_1000_READ));
 if(loading){
  f.rdi=M+0x7000;u32(f.rdi+0x50,bits(2));CHECK(!call(NS_BETTER_TIMER));CHECK(!call(NS_TIMER_AOB));
 }else{bad=UINT64_MAX;f.rax=M+0x7000;u32(f.rax+0x54,1);CHECK(!call(NS_LOADED_AREA));}
 CHECK(r.inputs.ready&&!r.inputs.state.loadscreenflag&&!r.stats.calls[NS_TIMESCALE_MULTIPLIER]);
 check_path(path,FLASH,0);
 CHECK(!r.camera_valid&&!r.camera_base&&!r.camera_attempt.x&&!r.camera_attempt.y&&!r.camera_attempt.z&&!r.av.alt_flash_red_address);
 CHECK(!r.inputs.state.camera.x&&!r.inputs.state.camera.y&&!r.inputs.state.camera.z);
 /* Shared flashlight state remains available to the independent head path. */
 CHECK(r.inputs.flashlight_address==FLASH&&(r.inputs.valid&RE7_INPUT_FLASHLIGHT)&&
  (r.inputs.observed&(1ULL<<NS_FLASHLIGHT_WRITE))&&r.inputs.state.flashlightflag==1);
 camera(20);
 if(path==0){check_path(path,FLASH,0);u32(M+0x1fc0,bits(.9098039269f));check_path(path,FLASH,1);}
 else if(path==1){check_path(path,FLASH,1);}
 else{
  check_path(path,FLASH,0);flashlight(FLASH+0x1000);
  check_path(path,FLASH,0);check_path(path,FLASH+0x1000,1);
  /* Failed FLASHLIGHT_WRITE capture cannot retain its previous authority. */
  f.rcx=UINT64_MAX;CHECK(!call(NS_FLASHLIGHT_WRITE));CHECK(!r.inputs.ready);
  check_path(path,FLASH+0x1000,0);flashlight(FLASH+0x1000);CHECK(r.inputs.ready);
  check_path(path,FLASH+0x1000,1);
 }
}
static void f6(void){
 initial();native_runtime_set_camera_enabled(&r,0);flashlight(FLASH+0x1000);
 CHECK(r.inputs.flashlight_address==FLASH+0x1000&&r.inputs.state.flashlightflag==1);
 for(unsigned path=0;path<5;path++){check_path(path,FLASH+0x1000,0);}
 native_runtime_set_camera_enabled(&r,1);r.view.firstpersonflag=0;
 for(unsigned path=0;path<5;path++){check_path(path,FLASH+0x1000,0);}
 camera(20);check_path(1,FLASH+0x1000,1);check_path(0,FLASH+0x1000,0);
 u32(M+0x1fc0,bits(.9098039269f));check_path(0,FLASH+0x1000,1);
 for(unsigned path=2;path<5;path++){check_path(path,FLASH+0x1000,0);}
 flashlight(FLASH+0x1000);
 for(unsigned path=2;path<5;path++){check_path(path,FLASH+0x1000,1);}
}
static void timescale_loss(void){
 initial();r.state.readjustxyzfp=2;native_av_tick(&r);CHECK(r.av.transition_frames==1);
 f.rbx=M+0x7000;CHECK(!call(NS_TIMESCALE_MULTIPLIER));CHECK(!r.inputs.ready);
 CHECK(!r.camera_base&&!r.camera_valid&&!r.camera_attempt.x&&!r.av.transition_frames);
 r.inputs.failed=0;ready();
 for(unsigned path=0;path<5;path++){check_path(path,FLASH,0);}
}
int main(int argc,char**argv){
 CHECK(argc==2);if(!strcmp(argv[1],"f6")){f6();}
 else if(!strcmp(argv[1],"timescale")){timescale_loss();}
 else{CHECK(strlen(argv[1])==2);unsigned path=(unsigned)(argv[1][1]-'0');CHECK(path<5);lost(path,argv[1][0]=='l');}
 puts("native AV lifecycle passed");return 0;
}
