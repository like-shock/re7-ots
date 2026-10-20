#ifndef RE7_NATIVE_HUD_H
#define RE7_NATIVE_HUD_H
#include "native_bridge.h"
struct NativeRuntime;
typedef struct {
 uint64_t display;
 uint64_t owner_links[3][6]; /* Last successful HUD transaction, not write authority. */
 uint32_t oxygen,area;
 uint8_t nahhudhidden,nahtrans,nahtalk,bombtalk,freezetalk,stopmainroomtalk,loading,have_owners;
} NativeHudState;
void native_hud_reset(struct NativeRuntime *);
int native_hud_dispatch(struct NativeRuntime *,unsigned,NativeFrame *,void *);
#endif
