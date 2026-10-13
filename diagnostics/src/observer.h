#ifndef RE7_OBSERVER_H
#define RE7_OBSERVER_H
#include "core.h"
#define OBS_VERSION "0.2.0"
#define OBS_MAX_LINKS 6
#define OBS_PERIOD_MS 100u
#define OBS_MAX_SAMPLES 9000u
#define OBS_MAX_MS 900000u
#define OBS_MAX_BYTES (16u*1024u*1024u)
#define OBS_MAX_SESSIONS 4u
#define OBS_TIMESTAMP 0x69C1FE87u
#define OBS_IMAGE_SIZE 161705984u
#define OBS_EXE_HASH "d550260029b51a580e206b9a12a4eb7f4a1b4dd230bba5defcca81da40597321"
typedef int (*ObsRead)(void*,u64,void*,usize);
typedef enum {OBS_OK,OBS_NULL,OBS_READ_FAILED,OBS_BAD_POINTER,OBS_CHANGED,OBS_INVALID_VALUE,OBS_UNSUPPORTED} ObsStatus;
typedef struct {ObsStatus status;u32 fail_step,generation;u64 raw;float xyz[3];} ObsValue;
typedef struct {u64 links[3][OBS_MAX_LINKS];u32 valid[3],generation[3];} ObsState;
typedef struct {ObsValue player,aim,inventory,interact;} ObsSample;
void obs_sample(ObsRead read,void*ctx,u64 base,int supported,ObsState*state,ObsSample*out);
const char*obs_status_name(ObsStatus status);
#endif
