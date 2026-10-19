#ifndef RE7_NATIVE_AV_H
#define RE7_NATIVE_AV_H
#include "native_bridge.h"
struct NativeRuntime;
typedef struct {
 uint64_t alt_flash_red_address;
 /* Effect authority is separate from head-only shared flashlight inputs. */
 uint64_t flashlight_address;
 uint32_t transition_frames;
} NativeAvState;
void native_av_reset(struct NativeRuntime *);
int native_av_flash_target(const struct NativeRuntime *,uint64_t,uint64_t);
void native_av_tick(struct NativeRuntime *);
int native_av_dispatch(struct NativeRuntime *,unsigned,NativeFrame *,void *);
#endif
