#include "probe_lifecycle.h"
#include <assert.h>
#include <stdio.h>
typedef struct{int verifies,creates,enables,disables,fail_verify,fail_create,fail_enable,fail_disable;} Fake;
static int verify(void*v){Fake*f=v;return ++f->verifies!=f->fail_verify;}
static int create(void*v,unsigned i){Fake*f=v;f->creates++;return (int)i!=f->fail_create;}
static int enable(void*v){Fake*f=v;f->enables++;return !f->fail_enable;}
static int disable(void*v){Fake*f=v;f->disables++;return !f->fail_disable;}
int main(void){ProbeOps ops={verify,create,enable,disable};
 for(int failure=0;failure<9;failure++){
  Fake f={.fail_create=-1};ProbeLifecycle s={0};
  if(failure==0)f.fail_verify=1;
  if(failure>=1&&failure<=5)f.fail_create=failure-1;
  if(failure==6)f.fail_verify=2;
  if(failure>=7)f.fail_enable=1;
  if(failure==8)f.fail_disable=1;
  assert(!probe_install(&s,&ops,&f));assert(s.attempted&&s.result!=PROBE_INSTALLED);
  if(failure<7)assert(!f.enables&&!f.disables);
  if(failure>=7)assert(f.enables==1&&f.disables==1&&s.retain_trampolines);
  assert(s.result==(failure==8?PROBE_ENABLE_FAILED_RESIDENT:failure==7?PROBE_ENABLE_FAILED_RESTORED:PROBE_REJECTED));
  int creates=f.creates;assert(!probe_install(&s,&ops,&f));assert(creates==f.creates);
 }
 Fake f={.fail_create=-1};ProbeLifecycle s={0};assert(probe_install(&s,&ops,&f));
 assert(s.result==PROBE_INSTALLED&&s.retain_trampolines&&s.created==5&&f.verifies==2);
 assert(!probe_install(&s,&ops,&f)&&f.enables==1);
 puts("lifecycle: failures, retained trampolines and one-shot installation PASS");return 0;
}
