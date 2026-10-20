#ifndef RE7_NATIVE_CONTROLLER_H
#define RE7_NATIVE_CONTROLLER_H
#include "native_bridge.h"
struct NativeRuntime;
typedef struct {
 int camera_button,shoulder_button;
 uint32_t armed,down,pending_camera,pending_shoulder;
 uint64_t owner[2],samples[2],seen[2],last[2],aim;
 uint32_t area;
 unsigned connected_mask;int connection_available;
} NativeControllerState;
/* All calls share the runtime lock. Hook dispatch/reset collect/invalidate;
 * only the worker supplies connectivity, polls absence and consumes actions. */
void native_controller_reset(struct NativeRuntime*);
void native_controller_connections(struct NativeRuntime*,int available,unsigned mask);
void native_controller_poll(struct NativeRuntime*,uint64_t now_ms);
void native_controller_take(struct NativeRuntime*,unsigned*,unsigned*);
int native_controller_dispatch(struct NativeRuntime*,unsigned,NativeFrame*,void*);
int native_controller_configured(const struct NativeRuntime*);
#endif
