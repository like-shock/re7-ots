#include "probe_capture.h"
.text
/* No calls, allocations, game-pointer dereferences, floating arithmetic or
 * changes to SIMD/x87 registers. Only private capture storage is written.
 * MinHook trampoline replays the displaced original instructions afterwards.
 * All exits restore the original RFLAGS and the four scratch registers. */
.macro DETOUR n
.globl probe_detour_\n
probe_detour_\n:
 pushfq
 push %rax
 push %rdx
 push %r10
 push %r11
 cmpl $3,probe_enabled(%rip)
 jne .Lreturn\n
 lea probe_queues+(\n*PROBE_QUEUE_BYTES)(%rip),%r10
 lock incq 24(%r10)
 xor %eax,%eax
 mov $1,%edx
 lock cmpxchg %edx,0(%r10)
 jne .Ldrop\n
 mov 8(%r10),%r11
 mov %r11,%rax
 sub 16(%r10),%rax
 cmp $PROBE_CAPACITY,%rax
 jae .Lfull\n
 and $(PROBE_CAPACITY-1),%r11
 imul $PROBE_RECORD_BYTES,%r11,%r11
 lea 40(%r10,%r11),%r11
 mov $1,%eax
 lock xadd %rax,probe_sequence(%rip)
 inc %rax
 mov %rax,0(%r11)
 mov probe_frame(%rip),%rax
 mov %rax,8(%r11)
 mov 24(%rsp),%rax
 mov %rax,16(%r11)
 mov %rbx,24(%r11)
 mov %rcx,32(%r11)
 mov 16(%rsp),%rax
 mov %rax,40(%r11)
 mov %rdi,48(%r11)
 mov %rsi,56(%r11)
 mov %rbp,64(%r11)
 lea 40(%rsp),%rax
 mov %rax,72(%r11)
 movss %xmm0,80(%r11)
 movss %xmm1,84(%r11)
 movss %xmm2,88(%r11)
 movss %xmm15,92(%r11)
 incq 8(%r10)
 movl $0,0(%r10)
 jmp .Lreturn\n
.Lfull\n:
 movl $0,0(%r10)
.Ldrop\n:
 lock incq 32(%r10)
.Lreturn\n:
 pop %r11
 pop %r10
 pop %rdx
 pop %rax
 popfq
 jmp *probe_trampolines+(\n*8)(%rip)
.endm
DETOUR 0
DETOUR 1
DETOUR 2
DETOUR 3
DETOUR 4
#ifndef _WIN32
.section .note.GNU-stack,"",@progbits
#endif
