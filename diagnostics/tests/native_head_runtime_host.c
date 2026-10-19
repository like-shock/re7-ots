/* Real adapter: full transform span, FP/TP gate, failure replay and F6 OFF. */
#include "native_runtime.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static unsigned char mem[0x30000];
static unsigned reads,writes;static int fail_write;
static int rd(void*c,uint64_t a,void*out,size_t n){
 (void)c;reads++;if(a==0x8fcb610&&n==8){uint64_t root=0x10000;memcpy(out,&root,8);return 1;}
 if(n==8&&(a==0x8fc8018||a==0x8fbc380||a==0x8fc17f0)){
  uint64_t root=a==0x8fc8018?0x20000:a==0x8fbc380?0x27000:0;memcpy(out,&root,8);return 1;
 }
 if(a>=sizeof mem||n>sizeof mem-a)return 0;
 memcpy(out,mem+a,n);return 1;
}
static int wr(void*c,uint64_t a,const void*in,size_t n){
 (void)c;writes++;if(fail_write||a>=sizeof mem||n>sizeof mem-a)return 0;memcpy(mem+a,in,n);return 1;
}
static void q(unsigned a,uint64_t v){memcpy(mem+a,&v,8);}
static void number(unsigned a,float v){memcpy(mem+a,&v,4);}
static float number_at(unsigned a){float v;memcpy(&v,mem+a,4);return v;}
int __attribute__((ms_abi)) native_dispatch(unsigned s,NativeFrame*f,void*fp){(void)s;(void)f;(void)fp;return 0;}
int main(void){
 NativeRuntime r;NativeFrame f={0},before;unsigned char fp[1024]={0},saved[1024];
 q(0x100a0,0x11000);q(0x110e0,0x12000);q(0x12078,0x13000);
 q(0x13010,0x14000);q(0x14018,0x15000);q(0x150c0,0x16000);number(0x1653c,1);
 q(0x200d8,0x21000);q(0x21018,0x22000);
 q(0x10020,0x23000);q(0x23088,0x24000);q(0x24010,0x25000);q(0x250a8,0x26000);
 q(0x27208,0x28000);q(0x28058,0x29000);q(0x29148,0x2a000);q(0x2a030,0x2b000);q(0x2b048,0x2c000);
 q(0x11068,0x1a000);q(0x1a088,0x1b000);q(0x1b010,0x1c000);q(0x1c0c0,0x1d000);q(0x1d0c0,0x1e000);
 native_runtime_init(&r,0x10000,rd,wr,NULL);
 native_runtime_set_camera_enabled(&r,0);
 assert(!native_runtime_dispatch(&r,NS_CUTSCENE_AUTOLOOK,&f,fp));
 native_set_xmm_scalar(fp,9,11);native_set_xmm_scalar(fp,0,22);
 native_set_xmm_scalar(fp,1,33);native_set_xmm_scalar(fp,2,44);
 for(unsigned i=0;i<4;i++){
  unsigned a=(unsigned[]){0x16000,0x16040,0x16080,0x16280}[i];f.rbx=a;before=f;memcpy(saved,fp,sizeof fp);
  assert(native_runtime_dispatch(&r,NS_HEAD_Y_WRITE,&f,fp));
  assert(number_at(a+0x30)==11&&number_at(a+0x34)==-1000&&number_at(a+0x38)==33&&number_at(a+0x3c)==44);
  assert(!memcmp(&before,&f,sizeof f)&&!memcmp(saved,fp,sizeof fp));
 }
 /* Missing camera readiness must preserve head-only flashlight collection,
  * including its OFF/ON shadow gate, while AV remains disabled. */
 assert(!r.inputs.ready&&r.head.shadow==0x1e034);
 f.rcx=0x1f000;f.rbp=0;assert(!native_runtime_dispatch(&r,NS_FLASHLIGHT_WRITE,&f,fp));
 assert(r.inputs.flashlight_address==0x1f060&&(r.inputs.valid&RE7_INPUT_FLASHLIGHT)&&r.inputs.state.flashlightflag==0);
 f.rbx=0x1e000;writes=0;assert(!native_runtime_dispatch(&r,NS_HEAD_Y_WRITE,&f,fp)&&!writes);
 f.rbp=1;assert(!native_runtime_dispatch(&r,NS_FLASHLIGHT_WRITE,&f,fp));
 assert(r.inputs.state.flashlightflag==1&&(r.inputs.observed&(1ULL<<NS_FLASHLIGHT_WRITE)));
 assert(native_runtime_dispatch(&r,NS_HEAD_Y_WRITE,&f,fp)&&number_at(0x1e034)==-1000);
 /* The unrelated transform and every camera site must pass through F6 OFF. */
 reads=writes=0;f.rbx=0x19000;
 assert(!native_runtime_dispatch(&r,NS_HEAD_Y_WRITE,&f,fp)&&!reads&&!writes);
 for(unsigned site=0;site<NATIVE_SITE_COUNT;site++)if(!native_head_only_site(site)){
  assert(!native_runtime_dispatch(&r,site,&f,fp)&&!reads&&!writes);
 }
 f.rbx=0x16000;fail_write=1;assert(!native_runtime_dispatch(&r,NS_HEAD_Y_WRITE,&f,fp));fail_write=0;
 number(0x1653c,2);writes=0;assert(!native_runtime_dispatch(&r,NS_HEAD_Y_WRITE,&f,fp)&&!writes);number(0x1653c,1);
 native_runtime_set_hide_head(&r,0);reads=writes=0;
 assert(!native_runtime_dispatch(&r,NS_HEAD_Y_WRITE,&f,fp)&&!reads&&!writes);
 native_runtime_set_hide_head(&r,1);assert(!native_runtime_dispatch(&r,NS_CUTSCENE_AUTOLOOK,&f,fp));
 assert(native_runtime_dispatch(&r,NS_HEAD_Y_WRITE,&f,fp));
 /* Full OTS dispatch in actual TP must never conceal any head transform. */
 native_runtime_set_camera_enabled(&r,1);r.inputs.valid=~0ULL;r.inputs.failed=0;r.inputs.state.player_available=1;
 r.inputs.ready=1;r.state.stopflag=0;r.state.internalfpflag=0;reads=writes=0;
 assert(!native_runtime_dispatch(&r,NS_HEAD_Y_WRITE,&f,fp));assert(!reads&&!writes&&r.view.firstpersonflag==0);
 /* Automatic FP may conceal after the next original target producer. */
 r.state.stopflag=1;assert(!native_runtime_dispatch(&r,NS_HEAD_Y_WRITE,&f,fp));
 assert(!native_runtime_dispatch(&r,NS_CUTSCENE_AUTOLOOK,&f,fp));
 r.inputs.valid=~0ULL;r.inputs.failed=0;r.inputs.state.player_available=1;r.inputs.ready=1;
 assert(native_runtime_dispatch(&r,NS_HEAD_Y_WRITE,&f,fp));
 /* Replacement owner cannot keep the old transform writable. */
 q(0x150c0,0x17000);writes=0;
 assert(!native_runtime_dispatch(&r,NS_HEAD_Y_WRITE,&f,fp)&&!writes);
 /* ID3 can arrive before the pointer producer. Once its player Z becomes
  * readable, head-only mode must recover without another character event. */
 q(0x150c0,0x16000);number(0x164bc,1);q(0x181e8,3);
 native_runtime_init(&r,0x10000,rd,wr,NULL);native_runtime_set_camera_enabled(&r,0);
 f.rdi=0x18000;assert(!native_runtime_dispatch(&r,NS_CHARACTER_CMP,&f,fp));
 assert(!(r.inputs.valid&RE7_INPUT_CHARACTER));
 assert(!native_runtime_dispatch(&r,NS_CUTSCENE_AUTOLOOK,&f,fp));
 f.rbx=0x16000;assert(native_runtime_dispatch(&r,NS_HEAD_Y_WRITE,&f,fp));
 assert(r.inputs.state.characters==(1u<<RE7_ORIGINAL_CHAR_MAIN_CLANCY));
 /* Hair OFF must not turn the independently enabled head hiding OFF. */
 q(0x12080,0x1a000);q(0x1a010,0x1b000);q(0x1b018,0x1c000);q(0x1c0c0,0x1d000);q(0x181e8,2);
 native_runtime_init(&r,0x10000,rd,wr,NULL);native_runtime_set_camera_enabled(&r,0);
 f.rdi=0x18000;assert(!native_runtime_dispatch(&r,NS_CHARACTER_CMP,&f,fp));assert(!native_runtime_dispatch(&r,NS_CUTSCENE_AUTOLOOK,&f,fp));
 f.rbx=0x1d180;assert(native_runtime_dispatch(&r,NS_HEAD_Y_WRITE,&f,fp));assert(number_at(0x1d1b4)==-1000);
 native_runtime_set_hide_mia_hair(&r,0);number(0x1d1b4,22);writes=0;
 assert(!native_runtime_dispatch(&r,NS_HEAD_Y_WRITE,&f,fp)&&!writes&&number_at(0x1d1b4)==22);
 f.rbx=0x16000;assert(native_runtime_dispatch(&r,NS_HEAD_Y_WRITE,&f,fp));
 native_runtime_set_hide_head(&r,0);native_runtime_set_hide_mia_hair(&r,1);writes=0;
 assert(!native_runtime_dispatch(&r,NS_HEAD_Y_WRITE,&f,fp)&&!writes);f.rbx=0x1d180;assert(!native_runtime_dispatch(&r,NS_HEAD_Y_WRITE,&f,fp)&&!writes);
 puts("native head runtime passed");return 0;
}
