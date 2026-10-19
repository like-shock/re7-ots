#ifndef RE7_NATIVE_RUNTIME_H
#define RE7_NATIVE_RUNTIME_H
#include "native_bridge.h"
#include "native_sites.h"
#include "original_inputs.h"
#include "original_collision.h"
#include "original_interaction.h"
#include "original_model.h"
#include "native_head.h"
#include "native_av.h"
#include "native_screen.h"
#include "native_hud.h"
#include "native_fov.h"
typedef int (*NativeMemory)(void *,uint64_t,void *,size_t);
typedef int (*NativeWrite)(void *,uint64_t,const void *,size_t);
typedef struct {
 uint64_t calls[NATIVE_MAX_SITES],applied[NATIVE_MAX_SITES];
 uint64_t reads_failed,writes_failed,camera_matches,view_changes;
 uint64_t camera_base,input_valid,input_failed,input_observed,input_observed_high;
 uint64_t head_applied[3],head_target;
 uint64_t light_contrast_samples;
 uint32_t light_contrast_input_bits,light_contrast_output_bits;
 uint32_t frame,loadarea,character_raw,characters;
 uint8_t camera_enabled,hide_head,character_observed,model_marker_available,assumeheadless;
 float model_marker;
 uint8_t ready,firstperson,stopflag,internalfp,aim,blockedgun,melee_auto_disable,limit_light_contrast;
 float camera[3],clip[3],framerate;
} NativeSnapshot;
typedef struct NativeRuntime {
 uint64_t base,camera_base;
 NativeMemory read; NativeWrite write; void *context;
 Re7OriginalParams params;
 Re7OriginalViewState view;
 Re7OriginalFocusState focus;
 Re7OriginalStateConfig config;
 Re7OriginalState state;
 Re7OriginalStateRates rates;
 Re7OriginalInputs inputs;
 Re7OriginalCollisionConfig collision_config;
 Re7OriginalCollisionState collision;
 Re7OriginalInteractionInput interaction_input;
 Re7OriginalInteractionState interaction;
 NativeHeadTargets head;
 NativeAvState av;
 NativeScreenState screen;
 NativeHudState hud;
 NativeFovState fov;
 uint8_t camera_enabled,hide_head,limit_light_contrast;
 Re7OriginalVec camera_attempt;
 float yawx,yawz,pitch;
 uint32_t fov_frame;
 uint8_t camera_valid,have_fov_frame;
 NativeSnapshot stats;
} NativeRuntime;
/* Caller serializes EVERY dispatch/control/snapshot. 0 replays the original
 * instruction span; 1 means its full memory/register effects were emulated. */
void native_runtime_init(NativeRuntime *,uint64_t,NativeMemory,NativeWrite,void*);
int native_runtime_dispatch(NativeRuntime *,unsigned,NativeFrame *,void *);
void native_runtime_shoulder(NativeRuntime *);
void native_runtime_set_melee_auto_disable(NativeRuntime *,int enabled);
void native_runtime_set_camera_enabled(NativeRuntime *,int enabled);
void native_runtime_set_hide_head(NativeRuntime *,int enabled);
void native_runtime_set_light_contrast(NativeRuntime *,int enabled);
void native_runtime_snapshot(const NativeRuntime *,NativeSnapshot *);
#endif
