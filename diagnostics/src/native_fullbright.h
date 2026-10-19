#ifndef RE7_NATIVE_FULLBRIGHT_H
#define RE7_NATIVE_FULLBRIGHT_H
#include "native_bridge.h"
struct NativeRuntime;
typedef struct {uint64_t light;uint32_t rgb;float brightness;uint8_t enabled,shadow_override;} NativeFullbrightState;
void native_fullbright_reset(struct NativeRuntime*);
int native_fullbright_dispatch(struct NativeRuntime*,unsigned,NativeFrame*,void*);
#endif
