#include "native_bridge.h"
.text
/* A detour arrives by JMP at game RSP=S, so [S] is NOT a return address.
 * The completed frame is 16-byte aligned and consists of:
 *   +0 NativeFrame(128), +128 immutable XMM6..15(160),
 *   +288 immutable nonvolatile GPRs(64), +352 machine frame(40).
 * UWOP_PUSH_MACHFRAME recovers original hook PC and S without a return pop.
 * Setup uses a temporary ordinary synthetic return slot until this frame is
 * complete. Restore has separate RSP-based pdata before R12 is overwritten.
 * No synthetic RET is executed. Static unwind metadata cannot describe the
 * very first PUSH before its slot exists or the final JMP after S is restored;
 * those tiny landing/transfer windows are explicitly outside this guarantee.
 * Real Windows/Proton exception dispatch/CET behavior remains hardware untested.
 */
#ifdef _WIN32
.macro MACHINE_UNWIND frame=0
 .seh_pushframe
 .seh_stackalloc 352
 .seh_savereg %rbx,288
 .seh_savereg %rbp,296
 .seh_savereg %rsi,304
 .seh_savereg %rdi,312
 .seh_savereg %r12,320
 .seh_savereg %r13,328
 .seh_savereg %r14,336
 .seh_savereg %r15,344
 .seh_savexmm %xmm6,128
 .seh_savexmm %xmm7,144
 .seh_savexmm %xmm8,160
 .seh_savexmm %xmm9,176
 .seh_savexmm %xmm10,192
 .seh_savexmm %xmm11,208
 .seh_savexmm %xmm12,224
 .seh_savexmm %xmm13,240
 .seh_savexmm %xmm14,256
 .seh_savexmm %xmm15,272
 .if \frame
 .seh_setframe %r12,0
 .endif
 .seh_endprologue
.endm
#endif
.macro RESTORE n,path
.globl native_restore_\path\()_\n
#ifdef _WIN32
.seh_proc native_restore_\path\()_\n
#endif
native_restore_\path\()_\n:
#ifdef _WIN32
 MACHINE_UNWIND 0
#endif
 mov 0(%rsp),%r15
 mov 8(%rsp),%r14
 mov 16(%rsp),%r13
 mov 32(%rsp),%r11
 mov 40(%rsp),%r10
 mov 48(%rsp),%r9
 mov 56(%rsp),%r8
 mov 64(%rsp),%rbp
 mov 72(%rsp),%rdi
 mov 80(%rsp),%rsi
 mov 88(%rsp),%rdx
 mov 96(%rsp),%rcx
 mov 104(%rsp),%rbx
 mov 112(%rsp),%rax
 mov 24(%rsp),%r12
 /* MOV preserves restored FLAGS; incoming S need not share frame alignment. */
 mov 376(%rsp),%rsp
#ifdef _WIN32
.seh_endproc
#endif
.globl native_transfer_\path\()_\n
native_transfer_\path\()_\n:
 .ifc \path,handled
 jmp *native_returns+(\n*8)(%rip)
 .else
 jmp *native_trampolines+(\n*8)(%rip)
 .endif
.endm
.macro DETOUR n
.globl native_detour_\n
native_detour_\n:
 /* First-instruction landing edge: no synthetic frame exists before PUSH. */
 pushq native_original_ips+(\n*8)(%rip)
.globl native_setup_\n
#ifdef _WIN32
.seh_proc native_setup_\n
#endif
native_setup_\n:
 pushfq
#ifdef _WIN32
 .seh_stackalloc 8
#endif
 push %rax
#ifdef _WIN32
 .seh_stackalloc 8
#endif
 push %rdx
#ifdef _WIN32
 .seh_stackalloc 8
#endif
 push %r12
#ifdef _WIN32
 .seh_pushreg %r12
#endif
 mov %rsp,%r12
#ifdef _WIN32
 .seh_setframe %r12,0
 .seh_endprologue
#endif
 /* R12=T=S-40 anchors the temporary return-slot frame while the aligned
  * machine frame is assembled. Fixed reserve is below one stack page. */
 lea -392(%rsp),%rdx
 and $-16,%rdx
 mov %rdx,%rsp
 mov %r15,0(%rsp)
 mov %r14,8(%rsp)
 mov %r13,16(%rsp)
 mov %r11,32(%rsp)
 mov %r10,40(%rsp)
 mov %r9,48(%rsp)
 mov %r8,56(%rsp)
 mov %rbp,64(%rsp)
 mov %rdi,72(%rsp)
 mov %rsi,80(%rsp)
 mov %rcx,96(%rsp)
 mov %rbx,104(%rsp)
 mov 0(%r12),%rax
 mov %rax,24(%rsp)
 mov %rax,320(%rsp)
 mov 8(%r12),%rax
 mov %rax,88(%rsp)
 mov 16(%r12),%rax
 mov %rax,112(%rsp)
 mov 24(%r12),%rax
 mov %rax,120(%rsp)
 mov %rax,368(%rsp)
 mov 32(%r12),%rax
 mov %rax,352(%rsp)
 lea 40(%r12),%rax
 mov %rax,376(%rsp)
 xor %eax,%eax
 mov %cs,%ax
 mov %rax,360(%rsp)
 xor %eax,%eax
 mov %ss,%ax
 mov %rax,384(%rsp)
 mov %rbx,288(%rsp)
 mov %rbp,296(%rsp)
 mov %rsi,304(%rsp)
 mov %rdi,312(%rsp)
 mov %r13,328(%rsp)
 mov %r14,336(%rsp)
 mov %r15,344(%rsp)
 movdqa %xmm6,128(%rsp)
 movdqa %xmm7,144(%rsp)
 movdqa %xmm8,160(%rsp)
 movdqa %xmm9,176(%rsp)
 movdqa %xmm10,192(%rsp)
 movdqa %xmm11,208(%rsp)
 movdqa %xmm12,224(%rsp)
 movdqa %xmm13,240(%rsp)
 movdqa %xmm14,256(%rsp)
 movdqa %xmm15,272(%rsp)
 /* At the following boundary the new frame is complete and R12 changes
  * meaning atomically. The next pdata entry describes that new meaning. */
 mov %rsp,%r12
#ifdef _WIN32
.seh_endproc
#endif
.globl native_body_\n
#ifdef _WIN32
.seh_proc native_body_\n
#endif
native_body_\n:
#ifdef _WIN32
 MACHINE_UNWIND 1
#endif
 cmpl $0,native_enabled(%rip)
 je native_prepare_original_\n
 .if (NATIVE_HEAD_ONLY_MASK & (1 << \n)) == 0
 cmpl $2,native_enabled(%rip)
 je native_prepare_original_\n
 .endif
 mov native_xsave_bytes(%rip),%eax
 add $192,%eax
.Lprobe\n:
 cmp $4096,%eax
 jb .Llast\n
 sub $4096,%rsp
 testb $0,(%rsp)
 sub $4096,%eax
 jmp .Lprobe\n
.Llast\n:
 sub %rax,%rsp
 and $-64,%rsp
 testb $0,(%rsp)
 lea 64(%rsp),%r13
 cmpl $0,native_use_xsave(%rip)
 je .Lfxsave\n
 movq $0,512(%r13)
 movq $0,520(%r13)
 movq $0,528(%r13)
 movq $0,536(%r13)
 movq $0,544(%r13)
 movq $0,552(%r13)
 movq $0,560(%r13)
 movq $0,568(%r13)
 mov native_xsave_mask(%rip),%eax
 mov native_xsave_mask+4(%rip),%edx
 xsave64 (%r13)
 jmp .Lcall\n
.Lfxsave\n:
 fxsave64 (%r13)
.Lcall\n:
 cld
 fninit
 ldmxcsr native_mxcsr(%rip)
 mov $\n,%ecx
 mov %r12,%rdx
 mov %r13,%r8
 call native_dispatch
 mov %eax,%r14d
 cmpl $0,native_use_xsave(%rip)
 je .Lfxrestore\n
 mov native_xsave_mask(%rip),%eax
 mov native_xsave_mask+4(%rip),%edx
 xrstor64 (%r13)
 jmp .Lresult\n
.Lfxrestore\n:
 fxrstor64 (%r13)
.Lresult\n:
 test %r14d,%r14d
 jz native_prepare_original_\n
 /* R12 still describes the complete frame across the temporary flags push. */
 pushq 120(%r12)
 popfq
 mov %r12,%rsp
#ifdef _WIN32
.seh_endproc
#endif
 RESTORE \n,handled
.globl native_prepare_original_\n
#ifdef _WIN32
.seh_proc native_prepare_original_\n
#endif
native_prepare_original_\n:
#ifdef _WIN32
 MACHINE_UNWIND 1
#endif
 pushq 120(%r12)
 popfq
 mov %r12,%rsp
#ifdef _WIN32
.seh_endproc
#endif
 RESTORE \n,original
.globl native_end_\n
native_end_\n:
.endm
.irp index,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55
 DETOUR \index
.endr
#ifndef _WIN32
.section .note.GNU-stack,"",@progbits
#endif
