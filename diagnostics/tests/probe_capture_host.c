#include "probe_capture.h"
#include <assert.h>
#include <cpuid.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
void exercise(void), test_trampoline(void);
void *test_hook;
uint64_t after_regs[15], after_flags, before_rsp, after_rsp;
unsigned char saved_fp[512] __attribute__((aligned(64)));
unsigned char before_fp[512] __attribute__((aligned(64)));
unsigned char after_fp[512] __attribute__((aligned(64)));
unsigned char test_vectors[512] __attribute__((aligned(64)));
unsigned char after_vectors[512] __attribute__((aligned(64)));
int have_avx;
int main(void) {
 unsigned a,b,c,d;__cpuid(1,a,b,c,d);
 if((c&(1u<<27))&&(c&(1u<<28))){unsigned x,y;__asm__("xgetbv":"=a"(x),"=d"(y):"c"(0));have_avx=(x&6)==6;}
 for(unsigned i=0;i<512;i++)test_vectors[i]=(unsigned char)(i*13+7);
 for(unsigned site=0;site<PROBE_SITE_COUNT;site++) {
  test_hook=probe_detours[site];probe_trampolines[site]=(void*)test_trampoline;
  for(unsigned path=0;path<4;path++) {
   memset(probe_queues,0,sizeof(probe_queues));probe_sequence=0;probe_frame=42;probe_enabled=path?3:0;
   ProbeQueue *q=&probe_queues[site];
   if(path==2)q->write=PROBE_CAPACITY;
   if(path==3)q->lock=1;
   exercise();
   for(unsigned i=0;i<15;i++)assert(after_regs[i]==0x120000+i);
   assert((after_flags&0xCD5)==(0x646&0xCD5));assert(before_rsp==after_rsp);
   assert(!memcmp(before_fp,after_fp,512));
   if(have_avx)assert(!memcmp(test_vectors,after_vectors,512));
   assert(q->calls==(path!=0));assert(q->drops==(path>=2));
   if(path==1){
    ProbeRecord e;assert(probe_drain(site,&e,1)==1);
    assert(e.sequence==1&&e.frame==42&&e.rax==0x120000&&e.rbx==0x120001);
    assert(e.rcx==0x120002&&e.rdx==0x120003&&e.rsi==0x120004&&e.rdi==0x120005&&e.rbp==0x120006);
    assert(e.rsp==before_rsp-8);
    assert(!memcmp(e.xmm,&test_vectors[0],4));assert(!memcmp(e.xmm+1,&test_vectors[32],4));
    assert(!memcmp(e.xmm+2,&test_vectors[64],4));assert(!memcmp(e.xmm+3,&test_vectors[480],4));
    assert(probe_drain(site,&e,1)==0);
   }
   if(path==3){ProbeRecord e;assert(probe_drain(site,&e,1)==0);assert(q->lock==1);q->lock=0;}
  }
  for(unsigned gate=0;gate<8;gate++){
   memset(probe_queues,0,sizeof(probe_queues));probe_enabled=gate;exercise();
   assert(probe_queues[site].calls==(gate==3));
  }
  memset(probe_queues,0,sizeof(probe_queues));probe_enabled=3;
  for(unsigned i=0;i<PROBE_CAPACITY;i++)exercise();
  exercise();assert(probe_queues[site].drops==1);
  ProbeRecord events[PROBE_CAPACITY];assert(probe_drain(site,events,PROBE_CAPACITY)==PROBE_CAPACITY);
  for(unsigned i=1;i<PROBE_CAPACITY;i++)assert(events[i].sequence==events[i-1].sequence+1);
  exercise();assert(probe_drain(site,events,1)==1); /* ring wraps */
 }
 puts("capture: all 5 detours and 4 paths PASS");return 0;
}
