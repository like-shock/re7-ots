/* Fail on a missing/wrong hook, floating-point instead of source DWORD
 * comparison, game writes, changed flags/registers, or an FP/loading override. */
#include "native_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static uint32_t source;
static unsigned reads,writes;
static int fail_read;
static int read_memory(void*c,uint64_t a,void*out,size_t n){
 (void)c;reads++;CHECK(a==0x30228&&n==4);
 if(fail_read)return 0;
 memcpy(out,&source,4);return 1;
}
static int write_memory(void*c,uint64_t a,const void*in,size_t n){
 (void)c;(void)a;(void)in;(void)n;writes++;return 0;
}
int __attribute__((ms_abi)) native_dispatch(unsigned s,NativeFrame*f,void*p){(void)s;(void)f;(void)p;return 0;}
static void ready(NativeRuntime*r){
 native_runtime_init(r,0x140000000,read_memory,write_memory,0);
 r->inputs.valid=RE7_INPUT_REQUIRED;r->inputs.state.player_available=1;
 reads=writes=0;fail_read=0;
}
int main(void){
 unsigned site=NATIVE_SITE_COUNT;
 for(unsigned i=0;i<NATIVE_SITE_COUNT;i++)
  if(!strcmp(native_sites[i].name,"OLD_HOUSE_BRIGHTNESS_READ"))site=i;
 CHECK(site<NATIVE_SITE_COUNT);
 CHECK(native_sites[site].rva==0x2E865D7&&native_sites[site].length==7);
 NativeRuntime r;NativeFrame f,original,expected;unsigned char fp[1024],saved_fp[1024];
 memset(&original,0x9b,sizeof original);original.r14=0x30000;
 memset(fp,0x6c,sizeof fp);memcpy(saved_fp,fp,sizeof fp);
 /* Literal IEEE-754 boundary from CT's (float)0.735; CMP/JBE are unsigned.
  * Negative zero, negative floats and NaNs intentionally retain CT semantics. */
 const struct {uint32_t input;int clamp;} cases[]={
  {0,0},{0x3e800000,0},{0x3f3c28f5,0},{0x3f3c28f6,0},
  {0x3f3c28f7,1},{0x3f800000,1},{0x7f800000,1},{0x7fc00001,1},
  {0x80000000,1},{0xbf800000,1},{0xff800000,1},{0xffc00001,1}};
 ready(&r);
 for(unsigned i=0;i<sizeof cases/sizeof cases[0];i++){
  source=cases[i].input;f=original;expected=original;reads=0;
  if(cases[i].clamp)expected.rax=0x3f3c28f6;
  CHECK(native_runtime_dispatch(&r,site,&f,fp)==cases[i].clamp);
  CHECK(reads==1&&!writes&&source==cases[i].input);
  CHECK(!memcmp(&f,&expected,sizeof f)&&!memcmp(fp,saved_fp,sizeof fp));
 }
 NativeSnapshot snap;native_runtime_snapshot(&r,&snap);
 CHECK(snap.light_contrast_samples==12&&snap.applied[site]==8);
 CHECK(snap.light_contrast_input_bits==0xffc00001&&snap.light_contrast_output_bits==0x3f3c28f6);
 source=0x3f800000;
 /* UI preference changes affect the next read; invalid values cannot enable it. */
 native_runtime_set_light_contrast(&r,0);native_runtime_set_light_contrast(&r,2);
 f=original;reads=0;native_runtime_snapshot(&r,&snap);CHECK(!snap.limit_light_contrast);
 CHECK(!native_runtime_dispatch(&r,site,&f,fp)&&!reads&&!writes);
 CHECK(!memcmp(&f,&original,sizeof f));
 native_runtime_set_light_contrast(&r,1);native_runtime_snapshot(&r,&snap);CHECK(snap.limit_light_contrast);
 CHECK(native_runtime_dispatch(&r,site,&f,fp)&&f.rax==0x3f3c28f6&&reads==1&&!writes);
 /* No area restriction: the source's OLD_HOUSE name is not a region gate. */
 for(unsigned area=0;area<40;area++){
  r.inputs.state.loadarea=area;f=original;reads=0;
  CHECK(native_runtime_dispatch(&r,site,&f,fp)&&reads==1&&f.rax==0x3f3c28f6);
 }

 /* Every gate replays the original, without reading the light or changing CPU state. */
 for(unsigned gate=0;gate<5;gate++){
  ready(&r);f=original;
  if(gate==0)r.view.firstpersonflag=1;
  if(gate==1)native_runtime_set_camera_enabled(&r,0);
  if(gate==2)r.inputs.valid&=~RE7_INPUT_PLAYER;
  if(gate==3)r.inputs.state.loadscreenflag=1;
  if(gate==4)r.inputs.failed=RE7_INPUT_AIM;
  CHECK(!native_runtime_dispatch(&r,site,&f,fp)&&!reads&&!writes);
  CHECK(!memcmp(&f,&original,sizeof f));
 }
 ready(&r);f=original;fail_read=1;
 CHECK(!native_runtime_dispatch(&r,site,&f,fp)&&reads==1&&!writes);
 CHECK(r.stats.reads_failed==1&&!memcmp(&f,&original,sizeof f));
 ready(&r);f=original;f.r14=UINT64_MAX-0x227;expected=f;
 CHECK(!native_runtime_dispatch(&r,site,&f,fp)&&!reads&&!writes);
 CHECK(!memcmp(&f,&expected,sizeof f));
 puts("native light contrast contract passed");return 0;
}
