#ifndef RE7_NATIVE_SCREEN_H
#define RE7_NATIVE_SCREEN_H
#include "native_bridge.h"
struct NativeRuntime;
typedef struct {
 uint64_t hp_address; /* Source HP+14 leaf, reacquired only at AUTOLOOK. */
 uint32_t hurtfilterframes,attemptedfilter,stoppedbloodheaddelay;
 uint32_t psychoactive; /* Producer=2, consumed only at CLOSE_DISTORTION cadence. */
} NativeScreenState;
void native_screen_reset(struct NativeRuntime *);
void native_screen_tick(struct NativeRuntime *);
void native_screen_observe_player(struct NativeRuntime *);
int native_screen_dispatch(struct NativeRuntime *,unsigned,NativeFrame *,void *);
#endif
