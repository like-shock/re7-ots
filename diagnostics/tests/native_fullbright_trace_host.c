#define main original_optional_cases
#include "native_optional_host.c"
#undef main
static NativeFullbrightEvent events[NATIVE_FULLBRIGHT_TRACE_CAPACITY];
static unsigned drain(void){return native_fullbright_take_events(&r,events,NATIVE_FULLBRIGHT_TRACE_CAPACITY);}
static void transitions(void){
 setup();CHECK(native_runtime_set_fullbright(&r,1,0xffffff,1));
 CHECK(call(NS_LIGHT_SOURCE_DIAMETER_READ));CHECK(call(NS_GLOBAL_BRIGHTNESS));
 CHECK(drain()>0);
 for(unsigned j=0;j<100;j++)CHECK(call(NS_LIGHT_SOURCE_DIAMETER_READ));
 CHECK(drain()==0); /* Stable hooks do not produce repeated event lines. */
 r.view.firstpersonflag=1;r.state.stopflag=1;
 CHECK(call(NS_G_BRIGHTNESS_2));CHECK(drain()==1);
 CHECK(events[0].firstperson==1&&events[0].stopflag==1&&events[0].gate==NFB_GATE_READY);
 CHECK(events[0].light==M+0x1000&&events[0].shadow_override);
 r.inputs.valid&=~RE7_INPUT_FOV;r.inputs.ready=0;
 save();CHECK(!call(NS_G_BRIGHTNESS_2));same();CHECK(!writes);
 CHECK(drain()==1&&events[0].gate==NFB_GATE_CAMERA_INPUTS&&!events[0].light);
 NativeSnapshot s;native_runtime_snapshot(&r,&s);
 CHECK(s.fullbright_failures[NFB_FAILURE_READINESS]==1);
 CHECK(s.fullbright_state.input_valid==r.inputs.valid);
 ready();CHECK(call(NS_G_BRIGHTNESS_2));CHECK(drain()==1&&events[0].gate==NFB_GATE_READY);
 CHECK(events[0].light==0); /* Readiness recovery alone does not invent a light. */
 f.rax=M+0x1000;CHECK(call(NS_LIGHT_SOURCE_DIAMETER_READ));
 CHECK(drain()==1&&events[0].light==M+0x1000);
}
static void failures(void){
 for(unsigned kind=0;kind<6;kind++){
  setup();CHECK(native_runtime_set_fullbright(&r,1,0xffffff,1));CHECK(call(NS_LIGHT_SOURCE_DIAMETER_READ));drain();
  unsigned reason=NFB_FAILURE_NONE;
  switch(kind){
  case 0:bad=M+0x1068;reason=NFB_FAILURE_MARKER_READ;break;
  case 1:u32(M+0x1068,0);reason=NFB_FAILURE_IDENTITY;break;
  case 2:bad=M+0x1080;reason=NFB_FAILURE_OPERANDS;break;
  case 3:{uint64_t alias=M+0x10a0-0x5c;memcpy(frame.full+NATIVE_FRAME_GAME_RSP_OFFSET,&alias,8);reason=NFB_FAILURE_STACK_OVERLAP;break;}
  case 4:fail_write=1;reason=NFB_FAILURE_STACK_WRITE;break;
  default:bad=M+0x1068;reason=NFB_FAILURE_RADIUS_IDENTITY;break;
  }
  save();CHECK(!call(kind==5?NS_LIGHT_RADIUS_MOVSS_READ:NS_LIGHT_SOURCE_DIAMETER_READ));same();
  CHECK(drain()==1&&events[0].last_failure==reason);
  NativeSnapshot s;native_runtime_snapshot(&r,&s);CHECK(s.fullbright_failures[reason]==1);
 }
 setup();CHECK(native_runtime_set_fullbright(&r,1,0xffffff,1));CHECK(call(NS_GLOBAL_BRIGHTNESS));drain();
 bad=f.r14+0x180;save();CHECK(!call(NS_GLOBAL_BRIGHTNESS));same();
 CHECK(drain()==1&&events[0].last_failure==NFB_FAILURE_GLOBAL_READ&&!events[0].shadow_override);
}
static void independent_fp(void){
 fp_setup();CHECK(!call(NS_G_BRIGHTNESS_2));CHECK(drain()>0);
 CHECK(events[0].gate==NFB_GATE_OBSERVERS);
 fp_producers();drain();CHECK(call(NS_LIGHT_SOURCE_DIAMETER_READ));
 CHECK(drain()==1&&events[0].gate==NFB_GATE_READY&&!events[0].camera_enabled);
 u32(M+0xe150,bits(0));f.rdi=M+0xe100;CHECK(!call(NS_BETTER_TIMER));
 CHECK(drain()==1&&events[0].gate==NFB_GATE_TIMER);
 r.inputs.oldaim_address=0;CHECK(!call(NS_TIMER_AOB));
 CHECK(drain()==1&&events[0].gate==NFB_GATE_LOADING);
}
static void mode(void){
 setup();CHECK(native_runtime_set_fullbright(&r,1,0xffffff,1));CHECK(call(NS_LIGHT_SOURCE_DIAMETER_READ));drain();
 native_runtime_set_camera_enabled(&r,0);
 CHECK(drain()==1&&!events[0].camera_enabled&&events[0].firstperson==1);
 CHECK(events[0].gate==NFB_GATE_OBSERVERS&&!events[0].light&&events[0].last_failure==NFB_FAILURE_RESET);
 CHECK(native_runtime_set_fullbright(&r,0,0xffffff,1));
 CHECK(drain()==1&&events[0].gate==NFB_GATE_OFF&&!events[0].enabled);
 native_runtime_set_camera_enabled(&r,1);CHECK(drain()==0);
}
static void bounded(void){
 setup();CHECK(native_runtime_set_fullbright(&r,1,0xffffff,1));CHECK(call(NS_LIGHT_SOURCE_DIAMETER_READ));drain();
 for(unsigned j=0;j<100;j++){r.view.firstpersonflag^=1;CHECK(call(NS_G_BRIGHTNESS_2));}
 NativeSnapshot s;native_runtime_snapshot(&r,&s);
 CHECK(s.fullbright_events_dropped==100-NATIVE_FULLBRIGHT_TRACE_CAPACITY);
 CHECK(drain()==NATIVE_FULLBRIGHT_TRACE_CAPACITY&&drain()==0);
 CHECK(r.fullbright.light==M+0x1000&&r.stats.applied[NS_G_BRIGHTNESS_2]==100);
 /* Collection consumes diagnostic records only, not the lighting state. */
 save();CHECK(call(NS_LIGHT_RADIUS_MOVSS_READ));CHECK(native_get_xmm_scalar(fp,0)==1000);
 CHECK(!strcmp(native_fullbright_gate_name(NFB_GATE_READY),"ready"));
 CHECK(!strcmp(native_fullbright_failure_name(NFB_FAILURE_STACK_WRITE),"stack_write_failed"));
}
int main(int argc,char**argv){
 CHECK(argc==2);
 if(!strcmp(argv[1],"transitions"))transitions();
 else if(!strcmp(argv[1],"failures"))failures();
 else if(!strcmp(argv[1],"fp"))independent_fp();
 else if(!strcmp(argv[1],"bounded"))bounded();
 else if(!strcmp(argv[1],"mode"))mode();
 else CHECK(0);
 return 0;
}
