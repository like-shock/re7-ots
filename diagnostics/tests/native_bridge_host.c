#include "native_bridge.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
void exercise(void),test_original(void),test_handled(void);
void *test_hook;uint64_t after_regs[15],after_flags,before_rsp,after_rsp;
unsigned char saved_fp[512] __attribute__((aligned(64)));
unsigned char before_fp[512] __attribute__((aligned(64))),after_fp[512] __attribute__((aligned(64)));
unsigned char test_vectors[512] __attribute__((aligned(64))),after_vectors[512] __attribute__((aligned(64)));
int have_avx,exit_path,handled,modify;unsigned calls,want_site,game_stack_padding;
int __attribute__((ms_abi)) native_dispatch(unsigned site,NativeFrame*f,void*fp){
 uint64_t flags;__asm__ volatile("pushfq;pop %0":"=r"(flags));assert(!(flags&0x400));
 unsigned short cw;unsigned mx;__asm__ volatile("fnstcw %0;stmxcsr %1":"=m"(cw),"=m"(mx));assert(cw==0x37f&&mx==0x1f80);
 assert(site==want_site&&f->rax==0x120000&&f->rbx==0x120001&&f->r15==0x12000e);
 assert(!memcmp((char*)fp+160,test_vectors,16));calls++;
 assert(((uintptr_t)f&15)==0);
 uint64_t machine_pc,machine_rsp;memcpy(&machine_pc,(char*)f+352,8);memcpy(&machine_rsp,(char*)f+376,8);
 assert(machine_pc==(uintptr_t)native_original_ips[site]);assert(machine_rsp==before_rsp);
 assert(native_frame_game_rsp(f)==before_rsp);
 assert(*(uint64_t*)(uintptr_t)before_rsp==UINT64_C(0xBADBADBADBADBAD));
 for(unsigned reg=6;reg<16;reg++)assert(!memcmp((char*)f+128+(reg-6)*16,test_vectors+reg*32,16));
 assert(*(uint64_t*)((char*)f+320)==UINT64_C(0x12000b));
 __asm__ volatile("fninit;fld1;pxor %%xmm0,%%xmm0;pxor %%xmm1,%%xmm1":::"xmm0","xmm1");
 if(have_avx)__asm__ volatile("vzeroall":::"xmm0","xmm1","xmm2","xmm3","xmm4","xmm5","xmm6","xmm7","xmm8","xmm9","xmm10","xmm11","xmm12","xmm13","xmm14","xmm15");
 if(modify){f->rax=123;native_set_xmm_scalar(fp,0,42.0f);}
 if(modify==2){f->r12=456;native_set_xmm_scalar(fp,15,24.0f);assert(*(uint64_t*)((char*)f+320)==UINT64_C(0x12000b));}
 return handled;
}
int main(void){
 assert(native_cpu_init());have_avx=(native_xsave_mask&4)!=0;
 unsigned actual_xsave=native_use_xsave;
 for(unsigned j=0;j<512;j++)test_vectors[j]=(unsigned char)(j*13+7);
 for(unsigned mode=0;mode<2;mode++){
 native_use_xsave=mode?0:actual_xsave;have_avx=native_use_xsave&&(native_xsave_mask&4)!=0;modify=0;
 for(unsigned padding=0;padding<=8;padding+=8){game_stack_padding=padding;
 for(unsigned site=0;site<NATIVE_MAX_SITES;site++){
  want_site=site;test_hook=native_detours[site];native_trampolines[site]=(void*)test_original;native_returns[site]=(void*)test_handled;native_original_ips[site]=(void*)(uintptr_t)(UINT64_C(0x140000000)+site*0x1000);
  for(unsigned path=0;path<4;path++){
   native_enabled=path==3?2:path!=0;handled=path==2;calls=0;exit_path=0;exercise();
   assert(calls==(path==3?(site==6||site==7||site==15||site==23||site==32):(path!=0)));assert(exit_path==(path==2?2:1));
   for(unsigned i=0;i<15;i++)assert(after_regs[i]==0x120000+i);
   assert((after_flags&0xcd5)==(0x646&0xcd5));assert(before_rsp==after_rsp);
   assert(!memcmp(before_fp,after_fp,512));
   if(have_avx)assert(!memcmp(test_vectors,after_vectors,512));
  }
 }
 native_enabled=1;modify=1;handled=1;exercise();assert(after_regs[0]==123);
 float value;memcpy(&value,after_fp+160,4);assert(value==42.0f);
 for(unsigned i=164;i<176;i++)assert(after_fp[i]==0); /* movss memory load clears high 96 */
 for(unsigned i=1;i<15;i++)assert(after_regs[i]==0x120000+i);
 modify=2;exercise();assert(after_regs[11]==456);memcpy(&value,after_fp+160+15*16,4);assert(value==24.0f);
 modify=0;
 }
 }
 puts("native bridge: PASS");return 0;
}
