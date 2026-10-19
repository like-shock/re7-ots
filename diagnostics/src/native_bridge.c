#include "native_bridge.h"
#include <cpuid.h>
#include <string.h>
_Atomic unsigned native_enabled;
uint64_t native_xsave_mask;
unsigned native_xsave_bytes=512,native_use_xsave;
const unsigned native_mxcsr=0x1f80;
void *native_trampolines[NATIVE_MAX_SITES],*native_returns[NATIVE_MAX_SITES];
void *native_original_ips[NATIVE_MAX_SITES];
int native_cpu_init(void){
 unsigned a,b,c,d;native_use_xsave=0;native_xsave_mask=0;native_xsave_bytes=512;
 if(!__get_cpuid(1,&a,&b,&c,&d)||(d&(1u<<24))==0)return 0;
 if((c&(1u<<26))&&(c&(1u<<27))){
  unsigned lo,hi;__asm__ volatile("xgetbv":"=a"(lo),"=d"(hi):"c"(0));
  native_xsave_mask=((uint64_t)hi<<32)|lo;
  if(!__get_cpuid_count(0xd,0,&a,&b,&c,&d)||b<576||b>65536)return 0;
  native_xsave_bytes=(b+63u)&~63u;native_use_xsave=1;
 }
 return 1;
}
float native_get_xmm_scalar(const void*p,unsigned n){float f=0;if(n<16)memcpy(&f,(const char*)p+160+16*n,4);return f;}
void native_set_xmm_scalar(void*p,unsigned n,float v){
 if(n>=16)return;
 memset((char*)p+160+16*n,0,16);memcpy((char*)p+160+16*n,&v,4);
 if(native_use_xsave){uint64_t bv;memcpy(&bv,(char*)p+512,8);bv|=2;memcpy((char*)p+512,&bv,8);}
}
void native_get_xmm_vector(const void*p,unsigned n,float out[4]){
 if(n<16)memcpy(out,(const char*)p+160+16*n,16);
}
void native_set_xmm_vector(void*p,unsigned n,const float v[4]){
 if(n>=16)return;
 memcpy((char*)p+160+16*n,v,16);
 if(native_use_xsave){uint64_t bv;memcpy(&bv,(char*)p+512,8);bv|=2;memcpy((char*)p+512,&bv,8);}
}
#define SITES(X) X(0) X(1) X(2) X(3) X(4) X(5) X(6) X(7) X(8) X(9) X(10) X(11) X(12) X(13) X(14) X(15) X(16) X(17) X(18) X(19) X(20) X(21) X(22) X(23) X(24) X(25) X(26) X(27) X(28) X(29) X(30) X(31) X(32) X(33) X(34) X(35) X(36) X(37) X(38) X(39) X(40) X(41) X(42) X(43) X(44) X(45) X(46) X(47) X(48) X(49) X(50)
#define DECL(n) extern void native_detour_##n(void);
SITES(DECL)
#define ENTRY(n) (void*)native_detour_##n,
void *const native_detours[NATIVE_MAX_SITES]={SITES(ENTRY)};
