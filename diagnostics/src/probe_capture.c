#include "probe_capture.h"
ProbeQueue probe_queues[PROBE_SITE_COUNT];
_Atomic uint32_t probe_enabled;
_Atomic uint64_t probe_frame,probe_sequence;
void *probe_trampolines[PROBE_SITE_COUNT];
void probe_detour_0(void),probe_detour_1(void),probe_detour_2(void),probe_detour_3(void),probe_detour_4(void);
void *const probe_detours[PROBE_SITE_COUNT]={probe_detour_0,probe_detour_1,probe_detour_2,probe_detour_3,probe_detour_4};
size_t probe_drain(unsigned site,ProbeRecord *out,size_t cap){
 if(site>=PROBE_SITE_COUNT||!out||!cap)return 0;
 ProbeQueue*q=&probe_queues[site];
 if(atomic_exchange_explicit(&q->lock,1,memory_order_acquire))return 0;
 uint64_t r=atomic_load_explicit(&q->read,memory_order_relaxed);
 uint64_t w=atomic_load_explicit(&q->write,memory_order_relaxed);
 size_t n=0;
 while(r!=w&&n<cap){out[n++]=q->events[r&(PROBE_CAPACITY-1)];r++;}
 atomic_store_explicit(&q->read,r,memory_order_relaxed);
 atomic_store_explicit(&q->lock,0,memory_order_release);return n;
}
