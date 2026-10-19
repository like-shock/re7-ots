#ifndef RE7_NATIVE_BRIDGE_H
#define RE7_NATIVE_BRIDGE_H
#define NATIVE_MAX_SITES 51
/* Camera+head=1, head-only=2; zero replays every original block.
 * Head-only producers: HEAD, AUTOLOOK, CHARACTER, AREA, FLASHLIGHT. */
#define NATIVE_HEAD_ONLY_MASK 0x1008080c0
#ifndef __ASSEMBLER__
#include <stdint.h>
#include <stdatomic.h>
#include <stddef.h>
typedef struct {
 uint64_t r15,r14,r13,r12,r11,r10,r9,r8,rbp,rdi,rsi,rdx,rcx,rbx,rax,flags;
} NativeFrame;
_Static_assert(sizeof(NativeFrame)==128 && offsetof(NativeFrame,rax)==112,"native bridge ABI");
extern _Atomic unsigned native_enabled;
extern uint64_t native_xsave_mask;
extern unsigned native_xsave_bytes,native_use_xsave;
extern const unsigned native_mxcsr;
/* Original hook PCs are separate from handled continuations. Publish all
 * three tables before enabling any detour. Completed bridge frames use a
 * terminal machine frame to unwind JMP entry to that PC and original RSP.
 * First PUSH/final JMP windows and real Windows/CET exception dispatch remain
 * outside the host-verified unwind guarantee. */
extern void *native_trampolines[NATIVE_MAX_SITES],*native_returns[NATIVE_MAX_SITES];
extern void *native_original_ips[NATIVE_MAX_SITES];
extern void *const native_detours[NATIVE_MAX_SITES];
int native_cpu_init(void);
void native_set_xmm_scalar(void *extended_state,unsigned reg,float value);
float native_get_xmm_scalar(const void *extended_state,unsigned reg);
void native_get_xmm_vector(const void *,unsigned,float out[4]);
void native_set_xmm_vector(void *,unsigned,const float value[4]);
int __attribute__((ms_abi)) native_dispatch(unsigned,NativeFrame*,void*);
#endif
#endif
