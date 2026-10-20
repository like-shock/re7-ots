#ifndef RE7_NATIVE_FULLBRIGHT_H
#define RE7_NATIVE_FULLBRIGHT_H
#include "native_bridge.h"
struct NativeRuntime;
#define NATIVE_FULLBRIGHT_TRACE_CAPACITY 8u
enum {
 NFB_GATE_OFF,NFB_GATE_READY,NFB_GATE_CAMERA_INPUTS,NFB_GATE_OBSERVERS,
 NFB_GATE_INPUTS,NFB_GATE_PLAYER,NFB_GATE_AIM_OWNER,NFB_GATE_LOADING,NFB_GATE_TIMER
};
enum {
 NFB_FAILURE_NONE,NFB_FAILURE_RESET,NFB_FAILURE_READINESS,NFB_FAILURE_OWNER,
 NFB_FAILURE_MARKER_READ,NFB_FAILURE_IDENTITY,NFB_FAILURE_OPERANDS,
 NFB_FAILURE_STACK_OVERLAP,NFB_FAILURE_STACK_WRITE,NFB_FAILURE_RADIUS_IDENTITY,
 NFB_FAILURE_GLOBAL_READ,NFB_FAILURE_COUNT
};
typedef struct {
 uint64_t sequence,light,player,aim,input_valid,input_failed,observed;
 uint32_t frame,site,area;float game_time;
 uint8_t enabled,camera_enabled,firstperson,stopflag,internalfp,shadow_override,gate,last_failure;
} NativeFullbrightEvent;
typedef struct {
 uint64_t sequence,dropped,failures[NFB_FAILURE_COUNT];
 uint32_t failure_frame,failure_site;
 unsigned head,count;
 uint8_t last_failure,have_last;
 NativeFullbrightEvent last,events[NATIVE_FULLBRIGHT_TRACE_CAPACITY];
} NativeFullbrightTrace;
typedef struct {
 uint64_t light,player,aim,observed;uint32_t area,rgb;float brightness;
 uint8_t enabled,shadow_override;
 NativeFullbrightTrace trace;
} NativeFullbrightState;
void native_fullbright_reset(struct NativeRuntime*);
int native_fullbright_dispatch(struct NativeRuntime*,unsigned,NativeFrame*,void*);
/* These helpers inspect only owned state; no game reads, allocation or I/O. */
void native_fullbright_record(struct NativeRuntime*,unsigned site);
void native_fullbright_snapshot(const struct NativeRuntime*,NativeFullbrightEvent*);
unsigned native_fullbright_take_events(struct NativeRuntime*,NativeFullbrightEvent*,unsigned capacity);
const char*native_fullbright_gate_name(unsigned);
const char*native_fullbright_failure_name(unsigned);
#endif
