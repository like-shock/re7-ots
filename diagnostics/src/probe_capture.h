#ifndef RE7_PROBE_CAPTURE_H
#define RE7_PROBE_CAPTURE_H
#define PROBE_SITE_COUNT 5
#define PROBE_CAPACITY 128
#define PROBE_RECORD_BYTES 96
#define PROBE_QUEUE_BYTES (40 + PROBE_CAPACITY * PROBE_RECORD_BYTES)
#ifndef __ASSEMBLER__
#include <stdint.h>
#include <stddef.h>
#include <stdatomic.h>
typedef struct {
 uint64_t sequence,frame,rax,rbx,rcx,rdx,rdi,rsi,rbp,rsp;
 float xmm[4]; /* XMM0, 1, 2, 15 low lanes, unchanged by detour. */
} ProbeRecord;
typedef struct {
 _Atomic uint32_t lock;
 uint32_t padding;
 _Atomic uint64_t write,read,calls,drops;
 ProbeRecord events[PROBE_CAPACITY];
} ProbeQueue;
_Static_assert(sizeof(ProbeRecord)==PROBE_RECORD_BYTES,"assembly record ABI");
_Static_assert(sizeof(ProbeQueue)==PROBE_QUEUE_BYTES,"assembly queue ABI");
_Static_assert(offsetof(ProbeRecord,xmm)==80 && offsetof(ProbeQueue,events)==40,"assembly offsets");
extern ProbeQueue probe_queues[PROBE_SITE_COUNT];
/* Gate bitmask: 1 running, 2 foreground, 4 permanently stopped. Capture only 3. */
extern _Atomic uint32_t probe_enabled;
extern _Atomic uint64_t probe_frame,probe_sequence;
extern void *probe_trampolines[PROBE_SITE_COUNT];
extern void *const probe_detours[PROBE_SITE_COUNT];
size_t probe_drain(unsigned site,ProbeRecord *out,size_t capacity);
#endif
#endif
