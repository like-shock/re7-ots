/* Integrated RT 8.02 adapter. The scalar modules own source policy; this file
 * owns exact hook operands, pointer identity, and the replaced block's effects.
 * No Lua/SDK/model discovery, scene raycast, or new proximity-based FP switch. */
#include "native_runtime.h"
#include <string.h>
_Static_assert(NATIVE_HEAD_ONLY_MASK == ((1ULL<<NS_HEAD_Y_WRITE)|(1ULL<<NS_CUTSCENE_AUTOLOOK)|(1ULL<<NS_CHARACTER_CMP)|(1ULL<<NS_LOADED_AREA)|(1ULL<<NS_FLASHLIGHT_WRITE)),"head-only bridge sites");
static uint32_t bits(float f){uint32_t u;memcpy(&u,&f,4);return u;}
static float scalar(uint32_t u){float f;memcpy(&f,&u,4);return f;}
static int finite_value(float f){return (bits(f)&0x7f800000u)!=0x7f800000u;}
static int vec_valid(Re7OriginalVec v){return finite_value(v.x)&&finite_value(v.y)&&finite_value(v.z);}
static int read_at(NativeRuntime*r,uint64_t a,void*out,size_t n){
 if(!n||a>UINT64_MAX-n||!r->read||!r->read(r->context,a,out,n)){r->stats.reads_failed++;return 0;}return 1;
}
static int read_cb(void*c,uint64_t a,void*out,size_t n){return read_at(c,a,out,n);}
static int write_at(NativeRuntime*r,uint64_t a,const void*in,size_t n){
 if(!n||a>UINT64_MAX-n||!r->write||!r->write(r->context,a,in,n)){r->stats.writes_failed++;return 0;}return 1;
}
static int offset(uint64_t a,uint64_t delta,uint64_t*out){if(a>UINT64_MAX-delta)return 0;*out=a+delta;return 1;}
static int load(NativeRuntime*r,uint64_t a,uint64_t d,void*out,size_t n){uint64_t p;if(!offset(a,d,&p)){r->stats.reads_failed++;return 0;}return read_at(r,p,out,n);}
static int store(NativeRuntime*r,uint64_t a,uint64_t d,const void*in,size_t n){uint64_t p;if(!offset(a,d,&p)){r->stats.writes_failed++;return 0;}return write_at(r,p,in,n);}
/* Active CT paths load some operands after earlier stores. Batch emulation
 * below requires disjoint ranges; ambiguous/overflowed aliases replay the
 * original block. Test whole byte ranges, including unaligned partial overlap. */
static int disjoint(uint64_t src,uint64_t so,size_t sn,uint64_t dst,uint64_t to,size_t tn){
 uint64_t a,b;
 return offset(src,so,&a)&&offset(dst,to,&b)&&a<=UINT64_MAX-sn&&b<=UINT64_MAX-tn&&(a+sn<=b||b+tn<=a);
}
static int camera_basis(NativeRuntime*r,float*xx,float*zz,float*pp){
 float v[10];if(!r->camera_base||!read_at(r,r->camera_base,v,sizeof v)||!finite_value(v[0])||!finite_value(v[2])||!finite_value(v[9]))return 0;
 *xx=v[2];*zz=v[0];*pp=v[9];return 1;
}
static int live_camera(NativeRuntime*r,Re7OriginalVec*out){return r->camera_valid&&load(r,r->camera_base,0x30,out,sizeof *out)&&vec_valid(*out);}
static Re7OriginalCollisionFlags clip_flags(const NativeRuntime*r){
 Re7OriginalCollisionFlags f={r->view.firstpersonflag,1,r->config.noclipautodisable,1};return f;
}
static void sync_input(NativeRuntime*r){
 Re7OriginalStateInput*s=&r->inputs.state;
 s->firstpersonflag=r->view.firstpersonflag;s->recentfpflag=r->view.recentfpflag;
 s->itempickuppop=r->interaction.itempickuppop;s->frametrack=r->inputs.frametrack;
 Re7OriginalInteractionInput*i=&r->interaction_input;
 i->firstpersonflag=r->view.firstpersonflag;i->interactableflag=s->interactableflag;
 i->loadarea=s->loadarea;i->frametrack=r->inputs.frametrack;i->cplayerz=s->player.z;
 i->invflagadd=r->inputs.inventory_address;i->lockboxflag=s->lockboxflag;
}
void native_runtime_init(NativeRuntime*r,uint64_t base,NativeMemory read,NativeWrite write,void*ctx){
 memset(r,0,sizeof *r);r->base=base;r->read=read;r->write=write;r->context=ctx;
 r->camera_enabled=1;r->hide_head=1;r->limit_light_contrast=1;
 re7_original_defaults(&r->params);re7_original_state_defaults(&r->config);
 re7_original_inputs_init(&r->inputs);re7_original_collision_defaults(&r->collision_config,&r->collision);
 r->interaction_input.tpinteractables=r->interaction_input.fixpaperscan=r->interaction_input.fixwormbox=1;
 r->interaction_input.fixfprompt=r->interaction_input.fixmain=1;
 re7_original_state_rates(&r->config,&r->params,60,&r->rates);
}
void native_runtime_shoulder(NativeRuntime*r){r->params.sidedis=r->params.sidedis<0?1:-1;}
void native_runtime_set_melee_auto_disable(NativeRuntime*r,int enabled){
 if(enabled==0||enabled==1)r->config.fpmeleeflag=(uint8_t)enabled;
}
void native_runtime_set_camera_enabled(NativeRuntime*r,int enabled){
 if(enabled!=0&&enabled!=1)return;
 r->camera_enabled=(uint8_t)enabled;r->config.thirdpersonflag=(uint8_t)enabled;
 if(!enabled){r->view.firstpersonflag=1;r->camera_valid=0;r->camera_base=0;r->av=(NativeAvState){0};}
 native_head_clear(&r->head);
}
void native_runtime_set_hide_head(NativeRuntime*r,int enabled){
 if(enabled!=0&&enabled!=1)return;
 r->hide_head=(uint8_t)enabled;
 if(!enabled)native_head_clear(&r->head);
}
void native_runtime_set_light_contrast(NativeRuntime*r,int enabled){
 if(enabled==0||enabled==1)r->limit_light_contrast=(uint8_t)enabled;
}
void native_runtime_snapshot(const NativeRuntime*r,NativeSnapshot*s){
 *s=r->stats;s->camera_base=r->camera_base;s->input_valid=r->inputs.valid;s->input_failed=r->inputs.failed;s->input_observed=r->inputs.observed;
 s->frame=r->inputs.frametrack;s->loadarea=r->inputs.state.loadarea;s->ready=r->inputs.ready;
 s->firstperson=r->view.firstpersonflag;s->stopflag=r->state.stopflag;s->internalfp=r->state.internalfpflag;
 s->aim=r->inputs.state.aim;s->blockedgun=r->state.blockedgun;s->framerate=r->inputs.state.framerate;
 s->melee_auto_disable=r->config.fpmeleeflag;
 s->limit_light_contrast=r->limit_light_contrast;
 s->camera_enabled=r->camera_enabled;s->hide_head=r->hide_head;s->head_target=r->head.head;
 s->character_raw=r->inputs.pcharcflag;s->characters=r->inputs.state.characters;
 s->character_observed=(r->inputs.observed&(1ULL<<NS_CHARACTER_CMP))!=0;
 s->model_marker_available=r->inputs.state.model_marker_available;
 s->model_marker=r->inputs.state.model_marker;s->assumeheadless=r->state.assumeheadless;
 s->clip[0]=r->params.camclipbehinddis;s->clip[1]=r->params.camclipsidedis;s->clip[2]=r->params.camclipheightdis;
}
static int camera_write(NativeRuntime*r,NativeFrame*f,void*fp){
 if(f->rbx!=r->camera_base||!r->inputs.ready)return 0;
 if(!disjoint(f->rbp,0x30,4,f->rbx,0x24,4)||!disjoint(f->rbp,0x30,4,f->rbx,0x30,12))return 0;
 float yawx,yawz,pitch,w;
 if(!camera_basis(r,&yawx,&yawz,&pitch)||!load(r,f->rbp,0x30,&w,4))return 0;
 Re7OriginalVec attempted={native_get_xmm_scalar(fp,0),native_get_xmm_scalar(fp,1),native_get_xmm_scalar(fp,2)};
 pitch=re7_original_adjust_pitch(native_get_xmm_scalar(fp,15),r->params.tppitchadj);
 if(!vec_valid(attempted)||!finite_value(pitch))return 0;
 /* CT's scene guards refer to cam*attempt (raw engine input), never the
  * displaced pose read later at CAM_AFTER_1000_READ. Keep those distinct. */
 r->camera_attempt=attempted;r->inputs.state.camera=attempted;
 r->stats.camera_matches++;
 /* Stage private transitions until all game stores succeed. X/Y intentionally
  * use the previous pitch multiplier; only Z sees its update and gun focus. */
 Re7OriginalParams p=r->params;Re7OriginalState state=r->state;Re7OriginalFocusState focus=r->focus;
 if(state.readjustxyzfp>=4)state.readjustxyzfp=0;
 int fp_xy=(state.readjustxyzfp<3&&r->view.firstpersonflag==1)||(r->config.fpaimflag==1&&r->inputs.state.aim==1);
 float out[4];out[0]=fp_xy?re7_original_fp_axis(RE7_ORIGINAL_X,attempted.x,yawx,yawz,&p):re7_original_ots_axis(RE7_ORIGINAL_X,attempted.x,yawx,yawz,pitch,&p);
 out[1]=(fp_xy||r->config.thirdpersonflag==0)?re7_original_fp_axis(RE7_ORIGINAL_Y,attempted.y,yawx,yawz,&p):re7_original_water_y(re7_original_ots_axis(RE7_ORIGINAL_Y,attempted.y,yawx,yawz,pitch,&p),r->inputs.state.loadarea,r->inputs.state.player);
 p.pitchmul=re7_original_pitch_multiplier(pitch);
 unsigned path=re7_original_state_camera(&r->config,&r->inputs.state,&state,&p,&focus,&r->rates);
 out[2]=path==RE7_ORIGINAL_CAMERA_FP?re7_original_fp_axis(RE7_ORIGINAL_Z,attempted.z,yawx,yawz,&p):re7_original_ots_axis(RE7_ORIGINAL_Z,attempted.z,yawx,yawz,pitch,&p);
 out[3]=w;
 if(!finite_value(out[0])||!finite_value(out[1])||!finite_value(out[2]))return 0;
 /* A failed store returns to the original full block, which rewrites every
  * destination, including any prefix a process write might have copied. */
 if(!store(r,f->rbx,0x24,&pitch,4)||!store(r,f->rbx,0x30,out,sizeof out))return 0;
 native_set_xmm_scalar(fp,0,w);
 r->params=p;r->state=state;r->focus=focus;
 r->yawx=yawx;r->yawz=yawz;r->pitch=pitch;r->camera_valid=1;
 memcpy(r->stats.camera,out,3*sizeof(float));return 1;
}
static int firing_write(NativeRuntime*r,unsigned site,NativeFrame*f){
 Re7OriginalVec camera;uint32_t z=0;
 if(site==NS_EOZ_AIM_POS_WRITE&&!disjoint(f->rax,8,4,f->rdi,0x530,8))return 0;
 if(!r->inputs.ready||!live_camera(r,&camera))return 0;
 if(site==NS_EOZ_AIM_POS_WRITE&&!load(r,f->rax,8,&z,4))return 0;
 if(!store(r,f->rdi,0x530,&camera,sizeof camera))return 0;
 if(site==NS_EOZ_AIM_POS_WRITE){f->rax=z;f->rcx=f->rbp+0x5d0;}return 1;
}
static int interaction_write(NativeRuntime*r,unsigned site,NativeFrame*f,void*fp){
 if(!r->inputs.ready||!r->camera_valid)return 0;
 if(site==NS_TP_INTERACTABLE_COORDS&&!disjoint(f->rbp,0x30,4,f->rbx,0x30,12))return 0;
 /* These are equivalent early exits of the source latch: a zero pickup latch
  * plus zero interaction flag cannot authorize a target. Avoid process reads
  * at the thousands of unrelated transform calls during normal walking. */
 if(site==NS_HEAD_Y_WRITE&&r->interaction.itempickuppop!=1){
  r->interaction.ivmainadd=0;
  if(r->inputs.state.interactableflag!=1)return 0;
 }
 if(site==NS_TP_INTERACTABLE_COORDS&&r->interaction.paperscanpickuppop!=1){
  r->interaction.pvmainadd=0;
  if(r->inputs.state.interactableflag!=1)return 0;
 }
 int selected=0;
 if(site==NS_TP_INTERACTABLE_COORDS)selected=re7_original_interaction_select(&r->interaction_input,&r->interaction,r->base,f->rbx,f->r13,read_cb,r);
 else {
  selected=re7_original_interaction_track_main(&r->interaction_input,&r->interaction,r->base,f->rbx,read_cb,r);
  /* track_main performs the original static-pointer/flag latch. Subsequent
   * note/grenade branches require that same successful interaction gate. */
  uint32_t active=0;
  int gate=r->interaction_input.tpinteractables==1&&r->view.firstpersonflag!=1&&r->interaction.itempickuppop==1&&load(r,r->base,0x8fb17f0,&active,4)&&active!=0;
  if(!selected&&gate){
   uint8_t lock=r->inputs.state.lockboxflag;
   selected=(lock==0||lock==5)&&f->r13==r->interaction_input.secintadd&&f->r15==2;
   if(!selected&&r->inputs.state.loadarea==35&&r->inputs.state.inventory==1&&f->rdi==r->interaction_input.secintadd&&f->r12==5){uint32_t v;selected=load(r,f->r9,0x14,&v,4)&&v==0;}
  }
 }
 if(!selected)return 0;
 float x,z,p,out[4];if(!camera_basis(r,&x,&z,&p))return 0;
 unsigned regs[3]={0,1,2};
 if(site==NS_HEAD_Y_WRITE){regs[0]=9;regs[1]=0;regs[2]=1;out[3]=native_get_xmm_scalar(fp,2);}
 else if(!load(r,f->rbp,0x30,&out[3],4))return 0;
 for(unsigned i=0;i<3;i++){float a=native_get_xmm_scalar(fp,regs[i]);if(!finite_value(a))return 0;out[i]=re7_original_ots_axis(i,a,x,z,p,&r->params);if(!finite_value(out[i]))return 0;}
 if(!store(r,f->rbx,0x30,out,sizeof out))return 0;
 if(site==NS_TP_INTERACTABLE_COORDS)native_set_xmm_scalar(fp,0,out[3]);
 return 1;
}
static int collision_write(NativeRuntime*r,unsigned site,NativeFrame*f,void*fp){
 if(!r->inputs.ready||!r->camera_valid)return 0;
 if(site!=NS_NAH_DOF_ORIGIN_ANGLE_WRITES&&(!disjoint(f->rax,4,4,f->rdi,0x560,4)||!disjoint(f->rax,8,4,f->rdi,0x560,8)))return 0;
 Re7OriginalCollisionInput in={0};in.flags=clip_flags(r);in.camera_attempt=r->camera_attempt;
 float pitch;in.available=(uint8_t)camera_basis(r,&in.yawx,&in.yawz,&pitch);
 in.framerate=r->inputs.state.framerate;in.autooldcamfov=r->inputs.state.autooldcamfov;
 if(site==NS_NAH_DOF_ORIGIN_ANGLE_WRITES)in.direction=(Re7OriginalVec){native_get_xmm_scalar(fp,0),native_get_xmm_scalar(fp,1),native_get_xmm_scalar(fp,2)};
 else {in.direction.x=scalar((uint32_t)f->rcx);if(!load(r,f->rax,4,&in.direction.y,4)||!load(r,f->rax,8,&in.direction.z,4))return 0;}
 Re7OriginalParams p=r->params;Re7OriginalCollisionState state=r->collision;Re7OriginalCollisionConfig config=r->collision_config;
 Re7OriginalCollisionOutput result=re7_original_collision_step(&p,&config,&state,&in);
 if(result.status!=RE7_ORIGINAL_COLLISION_APPLIED){r->params=p;r->collision=state;r->collision_config=config;return 0;}
 if(!store(r,f->rdi,0x560,&result.direction,sizeof result.direction))return 0;
 r->params=p;r->collision=state;r->collision_config=config;
 if(site==NS_NAH_DOF_ORIGIN_ANGLE_WRITES){native_set_xmm_scalar(fp,0,result.direction.x);native_set_xmm_scalar(fp,1,result.direction.y);native_set_xmm_scalar(fp,2,result.direction.z);}
 else{f->rcx=bits(result.direction.y);f->rax=bits(result.direction.z);}return 1;
}
static int model_write(NativeRuntime*r,unsigned site,NativeFrame*f){
 Re7OriginalModelInput in={r->base,f->rcx,1,1};Re7OriginalModelDecision out;
 if(site==NS_TRANS_GUN_WRITE){re7_original_model_gun_write(read_cb,r,&in,&out);return out.suppress_original_write!=0;}
 if(!load(r,f->rbp,0x10,&in.owner,8))return 0;
 re7_original_model_player_read(read_cb,r,&in,&out);
 if(!out.byte_write_address||!write_at(r,out.byte_write_address,&out.byte_write_value,1))return 0;
 uint32_t value;if(!load(r,in.owner,0x3d0,&value,4))return 0;f->rax=value;return 1;
}
static int flashlight_shadow_write(NativeRuntime*r,NativeFrame*f){
 /* RT 8.02 L10018-10038, tpflashshadowsdisable defaults to 1.
  * Only [rdi+C8] == flashflagadd-448 belongs to the original flashlight.
  * #9999 is a DWORD integer bit pattern, NOT float 9999. No light-color,
  * brightness or coordinate edits. A zero return replays MOV [rdi+C8],ecx. */
 uint64_t target,flag=r->inputs.flashlight_address;
 if(!r->inputs.ready||r->view.firstpersonflag==1||
    !(r->inputs.observed&(UINT64_C(1)<<NS_FLASHLIGHT_WRITE))||
    !(r->inputs.valid&RE7_INPUT_FLASHLIGHT)||flag<0x10000+0x448||
    f->rdi<0x10000||!offset(f->rdi,0xc8,&target)||target!=flag-0x448)return 0;
 const uint32_t value=9999;
 return write_at(r,target,&value,sizeof value);
}
static int light_contrast_read(NativeRuntime*r,NativeFrame*f){
 /* RT 8.02 L10198-10226, limittplightfade defaults to 1. The source
  * CMP/JBE compares unsigned DWORD bits, including negative/NaN encodings.
  * Substitute only MOV eax,[r14+228]'s result; never edit the light's field.
  * Fullbright's separate disableshadowdarken override is intentionally absent.
  * Native input readiness additionally prevents changes during loading/failure. */
 if(!r->limit_light_contrast||!r->inputs.ready||r->view.firstpersonflag==1)return 0;
 uint32_t value;const uint32_t cap=0x3f3c28f6u; /* float 0.735 */
 if(!load(r,f->r14,0x228,&value,sizeof value))return 0;
 r->stats.light_contrast_samples++;
 r->stats.light_contrast_input_bits=value;
 r->stats.light_contrast_output_bits=value>cap?cap:value;
 if(value<=cap)return 0;
 f->rax=cap; /* MOV eax zero-extends RAX. Flags/other registers stay intact. */
 return 1;
}
static int autolook(NativeRuntime*r,NativeFrame*f){
 if(!r->inputs.ready||r->view.firstpersonflag==1)return 0;
 static const unsigned offsets[]={0x78,0x20,0x590,0x40,0x30,0x178};uint64_t p,value;
 if(!load(r,r->base,0x8fb06e8,&p,8))return 0;
 for(unsigned i=0;i<sizeof offsets/sizeof offsets[0];i++)if(!load(r,p,offsets[i],&p,8))return 0;
 if(p!=f->rdx||!load(r,f->rcx,0x50,&value,8))return 0;
 f->rax=value;return 1;
}
static int head_allowed(const NativeRuntime*r){
 return r->hide_head&&r->view.firstpersonflag==1&&
  (!r->camera_enabled||(r->inputs.ready&&r->state.assumeheadless!=1))&&
  (r->inputs.valid&RE7_INPUT_CHARACTER)!=0;
}
static int head_write(NativeRuntime*r,NativeFrame*f,void*fp){
 if(!head_allowed(r))return 0;
 unsigned kind=native_head_match(&r->head,r->base,f->rbx,r->inputs.state.characters,
                                r->inputs.state.flashlightflag==1,read_cb,r);
 if(!kind)return 0;
 /* Original HEAD_Y_WRITE's complete 21-byte block. Only Y changes; X/Z/W,
  * all saved registers/flags and extended state remain the engine's values. */
 float out[4]={native_get_xmm_scalar(fp,9),-1000.0f,
               native_get_xmm_scalar(fp,1),native_get_xmm_scalar(fp,2)};
 if(!store(r,f->rbx,0x30,out,sizeof out))return 0;
 r->stats.head_applied[kind-1]++;return 1;
}
int native_runtime_dispatch(NativeRuntime*r,unsigned site,NativeFrame*f,void*fp){
 if(!r||!f||!fp||site>=NATIVE_SITE_COUNT)return 0;
 if(!r->camera_enabled&&!((NATIVE_HEAD_ONLY_MASK>>site)&1))return 0;
 r->stats.calls[site]++;
 if(site==NS_CAM_WRITES&&f->rbx!=r->camera_base)return 0;
 sync_input(r);
 re7_original_inputs_capture(&r->inputs,site,f,fp,r->base,read_cb,r,&r->config);
 r->view.recentfpflag=r->inputs.state.recentfpflag;sync_input(r);int handled=0;
 switch(site){
 case NS_CAM_AFTER_1000_READ:
  if(f->rdi>=0xa0){uint64_t candidate=f->rdi-0xa0;float values[16];
   if(read_at(r,candidate,values,sizeof values)&&finite_value(values[0])&&finite_value(values[2])&&finite_value(values[9])&&finite_value(values[12])&&finite_value(values[13])&&finite_value(values[14])){
    if(candidate!=r->camera_base){r->camera_valid=0;r->av.alt_flash_red_address=0;}
    r->camera_base=candidate;r->yawz=values[0];r->yawx=values[2];r->pitch=values[9];
    memcpy(r->stats.camera,values+12,3*sizeof(float));
   }else{r->camera_base=0;r->camera_valid=0;}
  }break;
 case NS_CAM_WRITES:handled=camera_write(r,f,fp);break;
 case NS_FIRING_COOR_WRITES:case NS_NAH_AIM_POS_WRITE:case NS_EOZ_AIM_POS_WRITE:handled=firing_write(r,site,f);break;
 case NS_HEAD_Y_WRITE:{
  uint8_t previous=r->view.firstpersonflag;
  Re7OriginalViewInput in={r->state.stopflag,r->state.internalfpflag,r->config.thirdpersonflag,r->config.fpaimflag,r->inputs.state.aim};
  if(!r->camera_enabled)r->view.firstpersonflag=1;
  else if(r->inputs.ready)re7_original_view(&in,&r->view);
  if(previous!=r->view.firstpersonflag)r->stats.view_changes++;
  if(r->view.firstpersonflag!=1)native_head_clear(&r->head);
  sync_input(r);handled=head_write(r,f,fp);
  if(!handled&&r->camera_enabled)handled=interaction_write(r,site,f,fp);
  break;
 }
 case NS_TP_INTERACTABLE_COORDS:handled=interaction_write(r,site,f,fp);break;
 case NS_DOF_DISTANCE_READ:{
  Re7OriginalCollisionReadInput in={0};in.flags=clip_flags(r);in.loadarea=r->inputs.state.loadarea;in.playerx=r->inputs.state.player.x;in.playerz=r->inputs.state.player.z;
  in.available=(uint8_t)load(r,f->rcx,0x18,&in.distance,4);re7_original_collision_distance_read(&r->collision,&in);break;
 }
 case NS_EXAMINE_DOF_WRITE:{Re7OriginalCollisionFlags flags=clip_flags(r);re7_original_collision_examine_write(&r->collision,&flags,native_get_xmm_scalar(fp,7),1);break;}
 case NS_DOF_ORIGIN_ANGLE_WRITES:case NS_EOZ_DOF_ORIGIN_ANGLE_WRITES:case NS_NAH_DOF_ORIGIN_ANGLE_WRITES:handled=collision_write(r,site,f,fp);break;
 case NS_PLAYER_TRANS_READ:case NS_TRANS_GUN_WRITE:if(r->inputs.ready)handled=model_write(r,site,f);break;
 case NS_FLASHLIGHT_SHADOWS_Z_WRITE:handled=flashlight_shadow_write(r,f);break;
 case NS_OLD_HOUSE_BRIGHTNESS_READ:handled=light_contrast_read(r,f);break;
 case NS_CUTSCENE_AUTOLOOK:
  if(head_allowed(r))native_head_refresh(&r->head,r->base,r->inputs.head_address,r->inputs.state.characters,read_cb,r);
  else native_head_clear(&r->head);
  if(r->camera_enabled)handled=autolook(r,f);
  break;
 case NS_INTERACTABLE_FLAG_ON:r->inputs.state.interactableflag=1;break;
 case NS_INTERACTABLE_FLAG_OFF:r->inputs.state.interactableflag=0;break;
 case NS_GET_SECONDARY_INTERACTABLE_R13_ADDRESS:{uint64_t a;if(re7_original_interaction_secondary(f->rax,read_cb,r,&a))r->interaction_input.secintadd=a;break;}
 case NS_FPROMPT_CMP:re7_original_interaction_prompt_seen(&r->interaction,r->inputs.state.interactableflag);break;
 case NS_INVENTORY_HIGHLIGHTED_ITEM_ID:r->interaction_input.scrapexamine=re7_original_interaction_scrap(f->rbx,read_cb,r);break;
 case NS_TIMESCALE_MULTIPLIER:
  native_av_tick(r);
  if(r->inputs.valid&RE7_INPUT_PLAYER)r->inputs.state.lockboxflag=re7_original_interaction_lockbox(r->inputs.state.loadarea,r->inputs.state.player.x,r->inputs.state.player.y,r->inputs.state.player.z);
  re7_original_interaction_loadscreen(&r->interaction,r->inputs.state.loadscreenflag);
  if(r->inputs.state.loadscreenflag){r->camera_valid=0;r->camera_base=0;r->av=(NativeAvState){0};}break;
 case NS_TRUE_FOV_READ:
  if(r->inputs.ready)re7_original_state_frame(&r->config,&r->inputs.state,&r->state,&r->params,&r->rates);
  break;
 case NS_AFTER_ADAPTIVE_DIFF:
  if(r->inputs.ready)re7_original_state_policy(&r->config,&r->inputs.state,&r->state);
  break;
 default:handled=native_av_dispatch(r,site,f,fp);break;
 }
 if(handled)r->stats.applied[site]++;
 return handled;
}
