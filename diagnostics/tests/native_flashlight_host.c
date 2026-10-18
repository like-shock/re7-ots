/* Drive the real adapter, including FLASHLIGHT_WRITE's address producer.
 * Removing the site, broadening its owner match, writing float 9999 rather
 * than DWORD 9999, or applying in FP must fail this contract. */
#include "native_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static unsigned char memory[0x80000];
static unsigned writes,reads;
static uint64_t last_address;
static size_t last_size;
static int fail_write;
static int read_memory(void*c,uint64_t a,void*out,size_t n){
 (void)c;reads++;if(a>sizeof memory||n>sizeof memory-a)return 0;
 memcpy(out,memory+a,n);return 1;
}
static int write_memory(void*c,uint64_t a,const void*in,size_t n){
 (void)c;writes++;last_address=a;last_size=n;
 if(fail_write||a>sizeof memory||n>sizeof memory-a)return 0;
 memcpy(memory+a,in,n);return 1;
}
int __attribute__((ms_abi)) native_dispatch(unsigned s,NativeFrame*f,void*p){(void)s;(void)f;(void)p;return 0;}
static void ready(NativeRuntime*r){
 native_runtime_init(r,0x140000000,read_memory,write_memory,0);
 r->inputs.valid=RE7_INPUT_REQUIRED|RE7_INPUT_FLASHLIGHT;
 r->inputs.state.player_available=1;r->inputs.ready=1;
 memset(memory,0xa5,sizeof memory);reads=writes=0;fail_write=0;
}
static uint32_t word(uint64_t a){uint32_t v;memcpy(&v,memory+a,4);return v;}
int main(void){
 unsigned site=NATIVE_SITE_COUNT;
 for(unsigned i=0;i<NATIVE_SITE_COUNT;i++)
  if(!strcmp(native_sites[i].name,"FLASHLIGHT_SHADOWS_Z_WRITE"))site=i;
 CHECK(site<NATIVE_SITE_COUNT);
 CHECK(native_sites[site].rva==0x2DEE559&&native_sites[site].length==6);
 NativeRuntime r;NativeFrame f,original;unsigned char fp[1024],saved_fp[1024];
 const uint64_t owner=0x30000,target=owner+0xc8;
 ready(&r);memset(&f,0x4b,sizeof f);memset(fp,0x6c,sizeof fp);
 f.rdi=owner;original=f;memcpy(saved_fp,fp,sizeof fp);
 /* No producer has identified the flashlight yet: never change another light. */
 CHECK(!native_runtime_dispatch(&r,site,&f,fp)&&!writes&&!reads);
 NativeFrame producer={0};producer.rcx=target+0x448-0x60;producer.rbp=1;
 CHECK(!native_runtime_dispatch(&r,NS_FLASHLIGHT_WRITE,&producer,fp));
 CHECK(r.inputs.flashlight_address==target+0x448);
 CHECK(!writes&&!reads);
 CHECK(native_runtime_dispatch(&r,site,&f,fp));
 CHECK(writes==1&&reads==0&&last_address==target&&last_size==4);
 CHECK(word(target)==9999&&memory[target-1]==0xa5&&memory[target+4]==0xa5);
 CHECK(!memcmp(&f,&original,sizeof f)&&!memcmp(fp,saved_fp,sizeof fp));
 NativeSnapshot snapshot;native_runtime_snapshot(&r,&snapshot);
 CHECK(snapshot.calls[site]==2&&snapshot.applied[site]==1);
 /* A nearby unrelated light, including a byte-offset mismatch, passes through. */
 for(unsigned i=1;i<=8;i++){
  f.rdi=owner+i;writes=reads=0;
  CHECK(!native_runtime_dispatch(&r,site,&f,fp)&&!writes&&!reads);
 }
 f=original;r.view.firstpersonflag=1;writes=0;
 CHECK(!native_runtime_dispatch(&r,site,&f,fp)&&!writes);
 r.view.firstpersonflag=0;
 /* Reacquisition must use the new producer immediately, not a cached target. */
 producer.rcx+=0x1000;producer.rbp=0;
 CHECK(!native_runtime_dispatch(&r,NS_FLASHLIGHT_WRITE,&producer,fp));
 CHECK(!native_runtime_dispatch(&r,site,&f,fp)&&!writes);
 f.rdi+=0x1000;CHECK(native_runtime_dispatch(&r,site,&f,fp));
 CHECK(word(target+0x1000)==9999); /* original does not gate on flashlight ON */
 writes=0;r.inputs.state.loadscreenflag=1;
 CHECK(!native_runtime_dispatch(&r,site,&f,fp)&&!writes);
 r.inputs.state.loadscreenflag=0;r.inputs.valid&=~RE7_INPUT_PLAYER;
 CHECK(!native_runtime_dispatch(&r,site,&f,fp)&&!writes);
 r.inputs.valid|=RE7_INPUT_PLAYER;fail_write=1;
 original=f;memcpy(saved_fp,fp,sizeof fp);
 CHECK(!native_runtime_dispatch(&r,site,&f,fp));
 CHECK(writes==1&&r.stats.writes_failed==1);
 CHECK(!memcmp(&f,&original,sizeof f)&&!memcmp(fp,saved_fp,sizeof fp));
 /* Invalid/overflowed owner arithmetic cannot alias the observed target. */
 fail_write=0;writes=0;f.rdi=UINT64_MAX-0xc7;
 CHECK(!native_runtime_dispatch(&r,site,&f,fp)&&!writes);
 r.inputs.flashlight_address=0x100;f.rdi=0;
 CHECK(!native_runtime_dispatch(&r,site,&f,fp)&&!writes);
 r.inputs.flashlight_address=0x10000+0x448;f.rdi=0x10000-0xc8;
 CHECK(!native_runtime_dispatch(&r,site,&f,fp)&&!writes);
 puts("native flashlight shadow contract passed");return 0;
}
