#ifndef RE7_PROBE_LIFECYCLE_H
#define RE7_PROBE_LIFECYCLE_H
#include "probe_capture.h"
typedef enum { PROBE_NEW,PROBE_REJECTED,PROBE_INSTALLED,PROBE_ENABLE_FAILED_RESTORED,PROBE_ENABLE_FAILED_RESIDENT } ProbeInstallResult;
typedef struct { int attempted,retain_trampolines;unsigned created;ProbeInstallResult result; } ProbeLifecycle;
typedef struct {
 int (*verify)(void*);int (*create)(void*,unsigned);int (*enable_all)(void*);int (*disable_all)(void*);
} ProbeOps;
/* One attempt. No remove/free callback: even a restored hook may have an
 * in-flight detour returning to its trampoline. DLL and buffers stay resident. */
int probe_install(ProbeLifecycle*,const ProbeOps*,void*);
#endif
