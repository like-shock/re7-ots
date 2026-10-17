#include "probe_lifecycle.h"
int probe_install(ProbeLifecycle*s,const ProbeOps*ops,void*ctx){
 if(s->attempted)return 0;
 s->attempted=1;s->result=PROBE_REJECTED;
 if(!ops->verify(ctx))return 0;
 for(unsigned i=0;i<PROBE_SITE_COUNT;i++){
  if(!ops->create(ctx,i))return 0;
  s->created++;s->retain_trampolines=1;
 }
 if(!ops->verify(ctx))return 0;
 if(!ops->enable_all(ctx)){
  s->result=ops->disable_all(ctx)?PROBE_ENABLE_FAILED_RESTORED:PROBE_ENABLE_FAILED_RESIDENT;
  return 0;
 }
 s->result=PROBE_INSTALLED;return 1;
}
