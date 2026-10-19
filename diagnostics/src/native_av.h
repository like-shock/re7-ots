#ifndef RE7_NATIVE_AV_H
#define RE7_NATIVE_AV_H
#include "native_bridge.h"
struct NativeRuntime;
typedef struct {
 uint64_t alt_flash_red_address;
 uint32_t transition_frames;
} NativeAvState;
void native_av_tick(struct NativeRuntime *);
int native_av_dispatch(struct NativeRuntime *,unsigned,NativeFrame *,void *);
#endif
