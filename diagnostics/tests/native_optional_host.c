#include "native_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"line %d: %s\n",__LINE__,#c);exit(1);}}while(0)
#define M UINT64_C(0x100000)
#define BASE UINT64_C(0x140000000)
static unsigned char mem[0x10000],fp[1024],saved_fp[1024];
static union{NativeFrame frame;unsigned char full[392];} frame;
#define f frame.frame
static NativeFrame saved;static NativeRuntime r;static uint64_t bad,change_at,root;
static unsigned writes,fail_write;static int changed,live_roots;
static void put(uint64_t a,const void*v,size_t n){CHECK(a>=M&&a-M+n<=sizeof mem);memcpy(mem+a-M,v,n);}
static void u32(uint64_t a,uint32_t v){put(a,&v,4);}static void u64(uint64_t a,uint64_t v){put(a,&v,8);}
static uint32_t bits(float v){uint32_t u;memcpy(&u,&v,4);return u;}
static uint32_t get(uint64_t a){uint32_t v;memcpy(&v,mem+a-M,4);return v;}
static int rd(void*c,uint64_t a,void*out,size_t n){
 (void)c;if(a==bad)return 0;
 if(live_roots&&n==8&&(a==BASE+0x8fb8018||a==BASE+0x8fbb610)){
  uint64_t value=a==BASE+0x8fb8018?M+0xc000:M+0xd000;memcpy(out,&value,8);return 1;
 }
 if(a==BASE+0x8ff2848&&n==8){memcpy(out,&root,8);return 1;}
 if(a<M||a-M>=sizeof mem||n>sizeof mem-(a-M))return 0;
 memcpy(out,mem+a-M,n);if(a==change_at&&!changed){u32(M+0x1068,0);changed=1;}return 1;
}
static int wr(void*c,uint64_t a,const void*in,size_t n){(void)c;writes++;if(writes==fail_write)return 0;if(a<M||a-M+n>sizeof mem)return 0;put(a,in,n);return 1;}
int __attribute__((ms_abi)) native_dispatch(unsigned s,NativeFrame*g,void*p){(void)s;(void)g;(void)p;return 0;}
static int call(unsigned site){return native_runtime_dispatch(&r,site,&f,fp);}
static void save(void){saved=f;memcpy(saved_fp,fp,sizeof fp);writes=0;}
static void same(void){CHECK(!memcmp(&saved,&f,sizeof f));CHECK(!memcmp(saved_fp,fp,sizeof fp));}
static void ready(void){r.inputs.valid=RE7_INPUT_REQUIRED|RE7_INPUT_TIMER;
 r.inputs.observed=(1ULL<<NS_CUTSCENE_AUTOLOOK)|(1ULL<<NS_LOADED_AREA)|(1ULL<<NS_BETTER_TIMER);r.inputs.ready=1;r.inputs.state.player_available=1;r.inputs.state.loadscreenflag=0;r.inputs.aim_address=M+0x9000;r.inputs.state.loadarea=1;}
static void setup(void){
 memset(mem,0,sizeof mem);memset(fp,0x53,sizeof fp);memset(&frame,0x72,sizeof frame);
 native_runtime_init(&r,BASE,rd,wr,0);native_controller_connections(&r,1,1);ready();native_use_xsave=1;fp[512]&=~2;
 bad=change_at=UINT64_MAX;writes=fail_write=changed=live_roots=0;f.flags=0xed7;
 f.rax=M+0x1000;f.rbx=M+0x1080;f.rcx=M+0x3000;f.rdi=M+0x2000;f.r14=M+0x4000;
 uint64_t rsp=M+0x8000;memcpy(frame.full+NATIVE_FRAME_GAME_RSP_OFFSET,&rsp,8);
 u32(M+0x1068,bits(150000));float data[16];for(unsigned j=0;j<16;j++){data[j]=(float)(j+1);}put(M+0x1080,data,sizeof data);u32(M+0x10d0,0xabcdef01);
 root=M+0x5000;u64(root+0x150,M+0x6000);u64(M+0x6050,M+0x7000);
}
/* Operand tests start after a valid lighting observation cycle. Lifecycle
 * tests below use the actual input producers instead of this fixture. */
static void light_fixture_observed(void){
 r.inputs.player_base=M+0xc400;r.inputs.oldaim_address=r.inputs.aim_address;
 r.inputs.state.igtimer=2;
 r.fullbright.observed=(1ULL<<NS_CUTSCENE_AUTOLOOK)|(1ULL<<NS_LOADED_AREA)|
  (1ULL<<NS_TIMER_AOB)|(1ULL<<NS_BETTER_TIMER);
}
static void shader(void){
 setup();u32(f.rdi+0x64,11);u32(f.rcx+0x14,0);u32(f.rcx+0x28,12);u32(f.rcx+0x38,0);
 CHECK(native_runtime_set_custom_filter(&r,1,1));save();CHECK(call(NS_FLASHBACK_FILTER_READ));
 CHECK(f.rax==1&&f.flags==((saved.flags&~0x8d5ULL)|0x95));saved=f;same();CHECK(r.screen.attemptedfilter==99);
 save();CHECK(call(NS_FLASHBACK_FILTER_READ_2));CHECK(f.rax==1&&f.flags==(saved.flags&~0x8d5ULL));
 for(int id=0;id<16;id++){CHECK(native_runtime_set_custom_filter(&r,1,id));CHECK(call(NS_FLASHBACK_FILTER_READ));CHECK(f.rax==(unsigned)id);}
 r.screen.hurtfilterframes=3;save();CHECK(!call(NS_FLASHBACK_FILTER_READ));same();CHECK(!call(NS_FLASHBACK_FILTER_READ_2));same();CHECK(r.screen.hurtfilterframes==3);
 r.screen.hurtfilterframes=0;bad=f.rcx+0x14;save();CHECK(!call(NS_FLASHBACK_FILTER_READ));same();
 bad=UINT64_MAX;CHECK(native_runtime_set_custom_filter(&r,0,1));save();CHECK(!call(NS_FLASHBACK_FILTER_READ));same();CHECK(r.screen.attemptedfilter==11);
}
static void light(void){
 setup();CHECK(native_runtime_set_fullbright(&r,1,0xffffff,1));light_fixture_observed();save();CHECK(call(NS_LIGHT_SOURCE_DIAMETER_READ));
 float a[]={1,2,3,1000},b[]={5,6,7,8},c[]={9,10,-1.9f,12},d[]={150000.015625f,150000.015625f,150000.015625f,-500};
 CHECK(f.rcx==0xabcdef01&&get(M+0x805c)==0xabcdef01&&writes==1);
 saved.rcx=0xabcdef01;memcpy(saved_fp+160+14*16,a,16);memcpy(saved_fp+160+15*16,b,16);memcpy(saved_fp+160+12*16,c,16);memcpy(saved_fp+160+11*16,d,16);saved_fp[512]|=2;same();
 CHECK(native_runtime_set_fullbright(&r,1,0x804020,2));CHECK(call(NS_LIGHT_SOURCE_DIAMETER_READ));
 /* x87 FLD 1/255, two FMULs, one final FSTP. */
 CHECK(native_get_xmm_scalar(fp,11)==150588.25f);
}
static void identity(void){
 setup();save();for(unsigned s=NS_LIGHT_SOURCE_DIAMETER_READ;s<=NS_G_BRIGHTNESS_2;s++){CHECK(!call(s));same();}CHECK(!writes);
 CHECK(native_runtime_set_fullbright(&r,1,0xffffff,1));light_fixture_observed();CHECK(call(NS_LIGHT_SOURCE_DIAMETER_READ));u32(M+0x1068,0);save();CHECK(!call(NS_LIGHT_RADIUS_MOVSS_READ));same();
 setup();CHECK(native_runtime_set_fullbright(&r,1,0xffffff,1));light_fixture_observed();change_at=M+0x1080;save();CHECK(!call(NS_LIGHT_SOURCE_DIAMETER_READ));same();CHECK(!writes);
 setup();CHECK(native_runtime_set_fullbright(&r,1,0xffffff,1));light_fixture_observed();uint64_t alias=M+0x10a0-0x5c;memcpy(frame.full+NATIVE_FRAME_GAME_RSP_OFFSET,&alias,8);save();CHECK(!call(NS_LIGHT_SOURCE_DIAMETER_READ));same();CHECK(!writes);
 setup();CHECK(native_runtime_set_fullbright(&r,1,0xffffff,1));light_fixture_observed();fail_write=1;save();CHECK(!call(NS_LIGHT_SOURCE_DIAMETER_READ));same();
 fail_write=0;CHECK(call(NS_LIGHT_SOURCE_DIAMETER_READ));r.inputs.state.loadscreenflag=1;save();CHECK(!call(NS_LIGHT_RADIUS_MOVSS_READ));same();r.inputs.state.loadscreenflag=0;ready();CHECK(!call(NS_LIGHT_RADIUS_MOVSS_READ));
}
static void consumers(void){
 setup();CHECK(native_runtime_set_fullbright(&r,1,0xffffff,1));light_fixture_observed();CHECK(call(NS_LIGHT_SOURCE_DIAMETER_READ));float vector[]={2,3,4,5};native_set_xmm_vector(fp,0,vector);save();CHECK(call(NS_LIGHT_RADIUS_MULSS_READ));vector[0]=2000;memcpy(saved_fp+160,vector,16);same();
 CHECK(call(NS_LIGHT_RADIUS_MOVSS_READ));CHECK(native_get_xmm_scalar(fp,0)==1000);uint32_t zero[3]={0};CHECK(!memcmp(fp+164,zero,12));
 f.rsi=f.rbx=M+0x7f80;save();CHECK(call(NS_EXAMINE_LIGHT_CMP_1));CHECK(f.flags==((saved.flags&~0x8d5ULL)|0x44));f.flags=saved.flags;CHECK(call(NS_EXAMINE_LIGHT_CMP_2));CHECK(f.flags==((saved.flags&~0x8d5ULL)|0x44));
 f.rbx++;save();CHECK(!call(NS_EXAMINE_LIGHT_CMP_2));same();bad=M+0x6050;CHECK(!call(NS_EXAMINE_LIGHT_CMP_1));same();bad=UINT64_MAX;
 float global[]={1,2,3,4};put(f.r14+0x180,global,16);save();CHECK(call(NS_GLOBAL_BRIGHTNESS));global[0]=.125f;memcpy(saved_fp+160,global,16);same();
 CHECK(call(NS_G_BRIGHTNESS_2));CHECK(native_get_xmm_scalar(fp,10)==.27f);CHECK(r.stats.calls[77]==1&&r.stats.applied[77]==1&&(r.inputs.observed_high&(1ULL<<13)));
 r.view.firstpersonflag=1;native_runtime_set_light_contrast(&r,0);CHECK(call(NS_OLD_HOUSE_BRIGHTNESS_READ));CHECK(f.rax==0);
 CHECK(native_runtime_set_fullbright(&r,0,0xffffff,1));save();CHECK(!call(NS_OLD_HOUSE_BRIGHTNESS_READ));same();CHECK(!r.limit_light_contrast);
 native_runtime_set_light_contrast(&r,1);r.view.firstpersonflag=0;u32(f.r14+0x228,bits(1));CHECK(call(NS_OLD_HOUSE_BRIGHTNESS_READ));CHECK(f.rax==0x3f3c28f6);
}
/* Camera-off lighting must bootstrap from actual producers, even with head
 * hiding and controller bindings OFF and no FOV/camera input ever observed. */
static void fp_setup(void){
 setup();re7_original_inputs_init(&r.inputs);native_runtime_set_camera_enabled(&r,0);
 native_runtime_set_hide_head(&r,0);CHECK(native_runtime_set_fullbright(&r,1,0xffffff,1));
 live_roots=1;u64(M+0xc0d8,M+0xc200);u64(M+0xc218,M+0xc400);
 u64(M+0xd020,M+0xd200);u64(M+0xd288,M+0xd400);
 u64(M+0xd410,M+0xd600);u64(M+0xd6a8,M+0xd800);
 u32(M+0xe054,1);u32(M+0xe150,bits(2));
}
static void fp_producers(void){
 save();CHECK(!call(NS_CUTSCENE_AUTOLOOK));same();CHECK(!writes);
 f.rax=M+0xe000;save();CHECK(!call(NS_LOADED_AREA));same();
 save();CHECK(!call(NS_TIMER_AOB));same();
 f.rdi=M+0xe100;save();CHECK(!call(NS_BETTER_TIMER));same();
 save();CHECK(!call(NS_TIMER_AOB));same();CHECK(!writes);
 f.rax=M+0x1000;f.rbx=M+0x1080;
}
static void fullbright_tp_inputs(void){
 fp_setup();native_runtime_set_camera_enabled(&r,1);fp_producers();
 /* Real lighting producers are sufficient without inventory/FOV readiness. */
 CHECK(r.camera_enabled&&!r.inputs.ready);
 CHECK(call(NS_LIGHT_SOURCE_DIAMETER_READ));CHECK(call(NS_GLOBAL_BRIGHTNESS));
 CHECK(call(NS_G_BRIGHTNESS_2));CHECK(native_get_xmm_scalar(fp,10)==.27f);
}
static void fullbright_auto_fp(void){
 fp_setup();native_runtime_set_camera_enabled(&r,1);fp_producers();
 const unsigned stops[]={0,1,0};
 for(unsigned j=0;j<3;j++){
  /* Supply the unrelated camera prerequisites, then use the real view path. */
  r.inputs.valid|=RE7_INPUT_REQUIRED;r.inputs.failed=0;
  r.state.stopflag=(uint8_t)stops[j];r.state.internalfpflag=0;
  f.rbx=M+0x2000;call(NS_HEAD_Y_WRITE);
  CHECK(r.inputs.ready&&r.camera_enabled&&r.view.firstpersonflag==stops[j]);
  f.rax=M+0x1000;f.rbx=M+0x1080;CHECK(call(NS_LIGHT_SOURCE_DIAMETER_READ));
  /* A real failed weapon read must suspend camera effects, not lighting. */
  f.rbx=M+0x2000;bad=f.rbx+0x3b0;CHECK(!call(NS_HELD_WEAPON_CMP));
  CHECK(!r.inputs.ready&&(r.inputs.failed&RE7_INPUT_WEAPON));
  save();CHECK(!call(NS_VIGNETTE));CHECK(!call(NS_CAM_AFTER_1000_READ));same();CHECK(!writes);
  f.rbx=M+0x1080;CHECK(call(NS_LIGHT_RADIUS_MOVSS_READ));
  CHECK(native_get_xmm_scalar(fp,0)==1000);
  CHECK(call(NS_GLOBAL_BRIGHTNESS));CHECK(native_get_xmm_scalar(fp,0)==.125f);
  CHECK(call(NS_G_BRIGHTNESS_2));CHECK(native_get_xmm_scalar(fp,10)==.27f);
  CHECK(call(NS_OLD_HOUSE_BRIGHTNESS_READ));CHECK(f.rax==0);
  f.rax=M+0x1000;CHECK(call(NS_LIGHT_SOURCE_DIAMETER_READ));
  CHECK(native_get_xmm_scalar(fp,11)==150000.015625f);
  bad=UINT64_MAX;f.rbx=M+0x2000;CHECK(!call(NS_HELD_WEAPON_CMP));CHECK(r.inputs.ready);
 }
}
static void fullbright_tp_activation(void){
 setup();CHECK(native_runtime_set_fullbright(&r,1,0xffffff,1));
 /* Camera readiness cannot substitute for this activation's observations. */
 save();CHECK(!call(NS_G_BRIGHTNESS_2));CHECK(!call(NS_GLOBAL_BRIGHTNESS));same();
 fp_setup();native_runtime_set_camera_enabled(&r,1);fp_producers();
 r.inputs.valid|=RE7_INPUT_REQUIRED;r.inputs.failed=0;
 CHECK(call(NS_G_BRIGHTNESS_2));CHECK(r.inputs.ready);
 CHECK(native_runtime_set_fullbright(&r,0,0xffffff,1));
 CHECK(native_runtime_set_fullbright(&r,1,0xffffff,1));
 save();CHECK(!call(NS_G_BRIGHTNESS_2));same();
 fp_producers();CHECK(call(NS_G_BRIGHTNESS_2));
}
/* CT keeps the examine-light identity when other scene lights are visited.
 * Both radius consumers must still see its expanded radius afterwards. */
static void fullbright_interleaved(int firstperson){
 if(firstperson){fp_setup();fp_producers();}
 else{setup();CHECK(native_runtime_set_fullbright(&r,1,0xffffff,1));light_fixture_observed();}
 CHECK(call(NS_LIGHT_SOURCE_DIAMETER_READ));
 for(unsigned j=0;j<3;j++){
  f.rax=M+0x2000+j*0x100;save();
  CHECK(!call(NS_LIGHT_SOURCE_DIAMETER_READ));same();CHECK(!writes);
  f.rbx=f.rax+0x80;save();
  CHECK(!call(NS_LIGHT_RADIUS_MOVSS_READ));same();
  CHECK(!call(NS_LIGHT_RADIUS_MULSS_READ));same();
  f.rbx=M+0x1080;
  float vector[]={2,3,4,5};native_set_xmm_vector(fp,0,vector);save();
  CHECK(call(NS_LIGHT_RADIUS_MULSS_READ));
  vector[0]=2000;memcpy(saved_fp+160,vector,16);same();
  CHECK(call(NS_LIGHT_RADIUS_MOVSS_READ));
  CHECK(native_get_xmm_scalar(fp,0)==1000);
 }
 /* Keeping a live light never authorizes a recycled/invalid target. */
 u32(M+0x1068,0);save();CHECK(!call(NS_LIGHT_RADIUS_MOVSS_READ));same();
 CHECK(!call(NS_LIGHT_RADIUS_MULSS_READ));same();
}
static void fullbright_cache_failures(void){
 for(int firstperson=0;firstperson<2;firstperson++)for(unsigned failure=0;failure<6;failure++){
  if(firstperson){fp_setup();fp_producers();}
  else{setup();CHECK(native_runtime_set_fullbright(&r,1,0xffffff,1));light_fixture_observed();}
  CHECK(call(NS_LIGHT_SOURCE_DIAMETER_READ));
  switch(failure){
  case 0:u32(M+0x1068,0);break;
  case 1:f.rax=M+0x2000;bad=f.rax+0x68;break;
  case 2:bad=M+0x1080;break;
  case 3:change_at=M+0x1080;break;
  case 4:fail_write=1;break;
  default:{uint64_t alias=M+0x10a0-0x5c;memcpy(frame.full+NATIVE_FRAME_GAME_RSP_OFFSET,&alias,8);break;}
  }
  save();CHECK(!call(NS_LIGHT_SOURCE_DIAMETER_READ));same();
  /* Restore readable memory without identifying the light anew. */
  bad=change_at=UINT64_MAX;fail_write=0;u32(M+0x1068,bits(150000));
  f.rbx=M+0x1080;save();CHECK(!call(NS_LIGHT_RADIUS_MOVSS_READ));same();
  CHECK(!call(NS_LIGHT_RADIUS_MULSS_READ));same();
 }
}
static void fullbright_fp(void){
 fp_setup();save();CHECK(!call(NS_G_BRIGHTNESS_2));same();
 fp_producers();CHECK(!r.inputs.ready&&r.view.firstpersonflag==1);
 CHECK(call(NS_LIGHT_SOURCE_DIAMETER_READ));CHECK(native_get_xmm_scalar(fp,11)==150000.015625f);
 CHECK(call(NS_LIGHT_RADIUS_MOVSS_READ));CHECK(native_get_xmm_scalar(fp,0)==1000);
 CHECK(call(NS_GLOBAL_BRIGHTNESS));CHECK(native_get_xmm_scalar(fp,0)==.125f);
 CHECK(call(NS_G_BRIGHTNESS_2));CHECK(native_get_xmm_scalar(fp,10)==.27f);
 CHECK(call(NS_OLD_HOUSE_BRIGHTNESS_READ));CHECK(f.rax==0);
 f.rsi=f.rbx=M+0x7f80;CHECK(call(NS_EXAMINE_LIGHT_CMP_1));CHECK(call(NS_EXAMINE_LIGHT_CMP_2));
 CHECK(native_runtime_set_fullbright(&r,0,0xffffff,1));
 save();for(unsigned site=71;site<78;site++){CHECK(!call(site));same();}
 CHECK(!call(NS_OLD_HOUSE_BRIGHTNESS_READ));same();
 CHECK(native_runtime_set_fullbright(&r,1,0x804020,2));fp_producers();f.rax=M+0x1000;
 CHECK(call(NS_LIGHT_SOURCE_DIAMETER_READ));CHECK(native_get_xmm_scalar(fp,11)==150588.25f);
 /* F6 ON then OFF retains the setting and still applies with no TP state. */
 native_runtime_set_camera_enabled(&r,1);native_runtime_set_camera_enabled(&r,0);
 CHECK(r.fullbright.enabled&&r.fullbright.rgb==0x804020&&r.fullbright.brightness==2);
 save();CHECK(!call(NS_G_BRIGHTNESS_2));same();fp_producers();
 f.rax=M+0x1000;CHECK(call(NS_LIGHT_SOURCE_DIAMETER_READ));
 /* Nothing outside the independent lighting/observer sites may run. */
 save();for(unsigned site=0;site<71;site++){
  if(site==7||site==14||site==23||site==25||site==42)continue;
  uint64_t before=r.stats.calls[site];CHECK(!call(site));same();CHECK(r.stats.calls[site]==before);
 }CHECK(!writes);
}
static void fullbright_loading(int camera){
 fp_setup();native_runtime_set_camera_enabled(&r,camera);fp_producers();CHECK(call(NS_LIGHT_SOURCE_DIAMETER_READ));CHECK(call(NS_GLOBAL_BRIGHTNESS));
 /* A replaced aim owner invalidates light/shadow before the next timer. */
 u64(M+0xd6a8,M+0xda00);CHECK(!call(NS_CUTSCENE_AUTOLOOK));
 CHECK(!r.fullbright.light&&!r.fullbright.shadow_override);
 f.rbx=M+0x1080;save();CHECK(!call(NS_LIGHT_RADIUS_MOVSS_READ));CHECK(!call(NS_OLD_HOUSE_BRIGHTNESS_READ));same();
 fp_producers();CHECK(!call(NS_LIGHT_RADIUS_MOVSS_READ));
 f.rax=M+0x1000;CHECK(call(NS_LIGHT_SOURCE_DIAMETER_READ));CHECK(call(NS_GLOBAL_BRIGHTNESS));
 /* A changed area also revokes cached consumers, despite a readable marker. */
 u32(M+0xe054,2);f.rax=M+0xe000;CHECK(!call(NS_LOADED_AREA));
 CHECK(!r.fullbright.light&&!r.fullbright.shadow_override);
 CHECK(!call(NS_LIGHT_RADIUS_MOVSS_READ));CHECK(!call(NS_OLD_HOUSE_BRIGHTNESS_READ));
 f.rax=M+0x1000;CHECK(call(NS_LIGHT_SOURCE_DIAMETER_READ));
 bad=M+0xc430;CHECK(!call(NS_CUTSCENE_AUTOLOOK));CHECK(!r.fullbright.light);
 save();CHECK(!call(NS_G_BRIGHTNESS_2));same();
 bad=UINT64_MAX;fp_producers();CHECK(!call(NS_LIGHT_RADIUS_MOVSS_READ));
 f.rax=M+0x1000;CHECK(call(NS_LIGHT_SOURCE_DIAMETER_READ));
 /* Game timer below the original load-complete threshold cannot light menus. */
 u32(M+0xe150,bits(0));f.rdi=M+0xe100;CHECK(!call(NS_BETTER_TIMER));
 CHECK(!r.fullbright.light&&!r.fullbright.shadow_override);save();CHECK(!call(NS_GLOBAL_BRIGHTNESS));same();
}
static void fullbright_loading_nah(int camera){
 fp_setup();native_runtime_set_camera_enabled(&r,camera);u32(M+0xe054,35);u32(M+0xe4d4,bits(2));
 CHECK(!call(NS_CUTSCENE_AUTOLOOK));f.rax=M+0xe000;CHECK(!call(NS_LOADED_AREA));
 f.rbx=M+0xe200;CHECK(!call(NS_TIMER_AOB));save();CHECK(!call(NS_G_BRIGHTNESS_2));same();
 CHECK(!call(NS_TIMER_AOB));CHECK(!(r.inputs.observed&(1ULL<<NS_BETTER_TIMER)));
 CHECK(call(NS_G_BRIGHTNESS_2));CHECK(native_get_xmm_scalar(fp,10)==.27f);
 f.rax=M+0x1000;CHECK(call(NS_LIGHT_SOURCE_DIAMETER_READ));CHECK(call(NS_GLOBAL_BRIGHTNESS));
 bad=M+0xe4d4;CHECK(!call(NS_TIMER_AOB));CHECK(!r.fullbright.light&&!r.fullbright.shadow_override);
 save();CHECK(!call(NS_G_BRIGHTNESS_2));CHECK(!call(NS_OLD_HOUSE_BRIGHTNESS_READ));same();
 bad=UINT64_MAX;CHECK(!call(NS_TIMER_AOB));CHECK(call(NS_G_BRIGHTNESS_2));
 /* Player replacement, even with unchanged aim/area, releases cached light. */
 f.rax=M+0x1000;CHECK(call(NS_LIGHT_SOURCE_DIAMETER_READ));CHECK(call(NS_GLOBAL_BRIGHTNESS));
 u64(M+0xc218,M+0xc600);CHECK(!call(NS_CUTSCENE_AUTOLOOK));
 CHECK(!r.fullbright.light&&!r.fullbright.shadow_override);
 f.rbx=M+0x1080;save();CHECK(!call(NS_LIGHT_RADIUS_MOVSS_READ));same();
}
static void fullbright_fp_reenable(void){
 fp_setup();fp_producers();CHECK(call(NS_G_BRIGHTNESS_2));
 CHECK(native_runtime_set_fullbright(&r,0,0xffffff,1));
 /* All independent observers can stop with F6/head/controller OFF. */
 live_roots=0;CHECK(!call(NS_CUTSCENE_AUTOLOOK));
 CHECK(native_runtime_set_fullbright(&r,1,0xffffff,1));
 save();CHECK(!call(NS_G_BRIGHTNESS_2));CHECK(!call(NS_GLOBAL_BRIGHTNESS));same();
 live_roots=1;fp_producers();CHECK(call(NS_G_BRIGHTNESS_2));
 /* Changing only RGB/brightness must not clear a live activation. */
 CHECK(native_runtime_set_fullbright(&r,1,0x804020,2));CHECK(call(NS_G_BRIGHTNESS_2));
}
static void press_owner(unsigned button,int down,uint64_t owner){
 f.rax=f.rbx=owner;
 if(button<16){u32(f.rax+0x24+4*button,down?0x80000000u:0);save();CHECK(!call(NS_XINPUT_FLOATS));same();}
 else{unsigned b=button-16;f.rax=down?(uint64_t)(b<4?(0x10u<<b)<<16:(1u<<(b-4))<<24):0;save();CHECK(!call(NS_XINPUT_WRITE));same();}
}
static void press(unsigned button,int down){press_owner(button,down,M+(button<16?0xa000:0xb000));}
static void actions(unsigned cam,unsigned shoulder){unsigned c,s;native_controller_take(&r,&c,&s);CHECK(c==cam&&s==shoulder);}
static void producer_owner(unsigned button){
 setup();CHECK(native_runtime_set_controller_bindings(&r,(int)button,(int)button));
 press(button,0);press(button,1);press(button,0);press(button,1);
 CHECK(r.controller.pending_camera==2&&r.controller.pending_shoulder==2);
 /* First observation of the other producer cannot invalidate this queue. */
 press(button<16?16:0,0);CHECK(r.controller.pending_camera==2&&r.controller.pending_shoulder==2);
 press_owner(button,1,M+0xc000);actions(0,0);
 press_owner(button,1,M+0xc000);actions(0,0);
 press_owner(button,0,M+0xc000);press_owner(button,1,M+0xc000);actions(1,1);
 press_owner(button,1,M+0xc000);actions(0,0);
}
static void buttons(void){
 for(unsigned b=0;b<24;b++){setup();CHECK(native_runtime_set_controller_bindings(&r,(int)b,(int)b));press(b,1);actions(0,0);press(b,0);press(b,1);actions(1,1);press(b,1);actions(0,0);press(b,0);press(b,1);actions(1,1);}
 setup();CHECK(native_runtime_set_controller_bindings(&r,16,20));press(16,0);press(20,0);f.rax=0x03300000;f.rbx=M+0xb000;CHECK(!call(NS_XINPUT_WRITE));actions(0,0);
}
static void controller_reset(void){
 setup();CHECK(native_runtime_set_controller_bindings(&r,0,-1));press(0,0);press(0,1);actions(1,0);
 CHECK(native_runtime_set_controller_bindings(&r,1,-1));press(1,1);actions(0,0);press(1,0);press(1,1);actions(1,0);
 CHECK(native_runtime_set_controller_bindings(&r,1,-1));native_runtime_set_hide_mia_hair(&r,0);CHECK(native_runtime_set_fullbright(&r,1,0x123456,2));press(1,1);actions(0,0);
 r.inputs.state.loadscreenflag=1;press(1,0);r.inputs.state.loadscreenflag=0;press(1,1);actions(0,0);press(1,0);press(1,1);actions(1,0);
 native_controller_poll(&r,100);native_controller_poll(&r,700);press(1,1);actions(0,0);press(1,0);press(1,1);actions(1,0);
 bad=M+0xa024;press(1,0);bad=UINT64_MAX;press(1,1);actions(0,0);
}
static void reconnect(void){
 setup();CHECK(native_runtime_set_controller_bindings(&r,0,-1));press(0,0);press(0,1);
 /* A disconnect may keep emitting readable zero samples. Those cannot arm. */
 native_controller_connections(&r,1,0);actions(0,0);press(0,0);press(0,0);native_controller_connections(&r,1,1);press(0,1);actions(0,0);press(0,0);press(0,1);actions(1,0);
 native_controller_connections(&r,1,3);press(0,1);actions(0,0);press(0,0);press(0,1);actions(1,0);
 native_controller_connections(&r,0,0);press(0,0);press(0,1);actions(0,0);native_controller_connections(&r,1,1);press(0,1);actions(0,0);
}
static void off(void){
 setup();CHECK(r.hide_mia_hair&&r.custom_shader==1&&!r.custom_filter&&!r.fullbright.enabled&&r.fullbright.rgb==0xffffff&&r.fullbright.brightness==1&&r.controller.camera_button==-1);
 CHECK(!native_runtime_set_custom_filter(&r,1,16));CHECK(!native_runtime_set_custom_filter(&r,2,1));CHECK(!native_runtime_set_fullbright(&r,1,0x1000000,1));CHECK(!native_runtime_set_fullbright(&r,1,0xffffff,-1));float nan;uint32_t u=0x7fc00000;memcpy(&nan,&u,4);CHECK(!native_runtime_set_fullbright(&r,1,0xffffff,nan));CHECK(!native_runtime_set_controller_bindings(&r,24,-1));
 native_runtime_set_camera_enabled(&r,0);native_runtime_set_hide_head(&r,0);CHECK(native_runtime_set_controller_bindings(&r,0,-1));ready();press(0,0);press(0,1);actions(1,0);
 CHECK(r.inputs.observed_high&(1ULL<<(NS_XINPUT_FLOATS-64)));save();CHECK(!call(NS_G_BRIGHTNESS_2));same();CHECK(!r.stats.calls[NS_G_BRIGHTNESS_2]);
}
int main(int argc,char**argv){CHECK(argc==2);if(!strcmp(argv[1],"shader"))shader();else if(!strcmp(argv[1],"light"))light();else if(!strcmp(argv[1],"identity"))identity();else if(!strcmp(argv[1],"consumers"))consumers();else if(!strcmp(argv[1],"fullbright_fp"))fullbright_fp();else if(!strcmp(argv[1],"fullbright_fp_loading"))fullbright_loading(0);else if(!strcmp(argv[1],"fullbright_fp_nah"))fullbright_loading_nah(0);else if(!strcmp(argv[1],"fullbright_fp_reenable"))fullbright_fp_reenable();else if(!strcmp(argv[1],"buttons"))buttons();else if(!strcmp(argv[1],"controller_reset"))controller_reset();else if(!strcmp(argv[1],"off"))off();else if(!strcmp(argv[1],"reconnect"))reconnect();else if(!strcmp(argv[1],"owner_floats"))producer_owner(0);else if(!strcmp(argv[1],"owner_write"))producer_owner(16);else if(!strcmp(argv[1],"fullbright_interleaved_tp"))fullbright_interleaved(0);else if(!strcmp(argv[1],"fullbright_interleaved_fp"))fullbright_interleaved(1);else if(!strcmp(argv[1],"fullbright_cache_failures"))fullbright_cache_failures();else if(!strcmp(argv[1],"fullbright_tp_inputs"))fullbright_tp_inputs();else if(!strcmp(argv[1],"fullbright_auto_fp"))fullbright_auto_fp();else if(!strcmp(argv[1],"fullbright_tp_activation"))fullbright_tp_activation();else if(!strcmp(argv[1],"fullbright_tp_loading"))fullbright_loading(1);else if(!strcmp(argv[1],"fullbright_tp_nah"))fullbright_loading_nah(1);else CHECK(0);return 0;}
