#ifndef RE7_NATIVE_FOV_H
#define RE7_NATIVE_FOV_H
#include <stdint.h>
struct NativeRuntime;
#include "native_bridge.h"
typedef struct {
 /* CT menu value is a copied signed dword, never a live UI pointer. */
 int32_t menu_index;
 uint8_t customfovflag,preventactionfovchange,fprunfov,tprunfov,nofovtamperflag;
 float custom_fov,runningfovmax,fovrunincbaserate,fovrundecbaserate;
 uint64_t reference,target;
 uint32_t reference_bits,target_bits,area,reframeworkfovframes;
 float runningfovadd,ratiorunfovmax;
 uint8_t runfovcalcflag,active,have_area;
} NativeFovState;
/* Drop transient ownership, preserving copied menu/settings and RF write age.
 * Only initial runtime construction starts RF age100; RF writes start age0. */
void native_fov_reset(struct NativeRuntime *);
int native_fov_dispatch(struct NativeRuntime *,unsigned,NativeFrame *,void *);
#endif
