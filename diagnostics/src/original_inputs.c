#include "original_inputs.h"
#include "native_sites.h"
#include "original_collision.h"
#include <string.h>

/* All addresses and source immediates are RT 8.02 hexadecimal offsets.
 * No heuristic RTTI search. Failed reads do not replace scalar snapshots. */
typedef struct { Re7OriginalInputs *i; Re7OriginalInputsRead read; void *ctx; } Access;
static uint32_t bits(float f) { uint32_t u; memcpy(&u,&f,4); return u; }
static int finite(float f) { return (bits(f)&0x7F800000u)!=0x7F800000u; }
static int span(uint64_t p,uint64_t off,size_t n,uint64_t *out) {
 const uint64_t hi=UINT64_C(0x7FFFFFFFFFFF);
 if(p<0x10000||p>hi||off>hi-p||!n)return 0;
 *out=p+off;return n-1<=hi-*out;
}
static void status(Re7OriginalInputs *i,uint64_t bit,int ok) {
 if(ok){i->valid|=bit;i->failed&=~bit;}else{i->valid&=~bit;i->failed|=bit;}
}
static int get(Access *a,uint64_t p,uint64_t off,void *dst,size_t n) {
 uint64_t address;
 if(!a->read||!span(p,off,n,&address)||!a->read(a->ctx,address,dst,n)){
  a->i->read_failures++;return 0;
 }
 return 1;
}
static int scalar(Access *a,uint64_t p,uint64_t off,float *dst) {
 float v;if(!get(a,p,off,&v,4)||!finite(v))return 0;*dst=v;return 1;
}
static int chain(Access *a,uint64_t base,uint32_t rva,const uint32_t *offsets,
                 unsigned count,uint32_t leaf,uint64_t *result) {
 uint64_t links[8],locations[8],p=base,next;
 if(count>7)return 0;
 for(unsigned j=0;j<=count;j++){
  if(!span(p,j?offsets[j-1]:rva,8,&locations[j])||
     !get(a,p,j?offsets[j-1]:rva,&next,8)||!span(next,0,1,&p))return 0;
  links[j]=p;
 }
 /* Recheck pointer links to reject concurrent replacement, including leaf
  * owner replacement. This cannot prove semantic identity or prevent ABA. */
 for(unsigned j=count+1;j>0;j--){
  if(!get(a,locations[j-1],0,&next,8)||next!=links[j-1])return 0;
 }
 return span(p,leaf,1,result);
}
static int byte(Access *a,uint64_t p,uint64_t off,uint8_t *v,uint64_t bit) {
 uint8_t next;int ok=get(a,p,off,&next,1);status(a->i,bit,ok);if(ok)*v=next;return ok;
}
static void player(Access *a) {
 Re7OriginalVec v;
 int ok=get(a,a->i->player_base,0x30,&v,12)&&finite(v.x)&&finite(v.y)&&finite(v.z);
 status(a->i,RE7_INPUT_PLAYER,ok);a->i->state.player_available=(uint8_t)ok;
 if(ok)a->i->state.player=v;
}
static void flags(Access *a,uint64_t base) {
 Re7OriginalInputs *i=a->i;uint64_t pointer;
 byte(a,i->aim_address,0,&i->state.aim,RE7_INPUT_AIM);
 byte(a,i->inventory_address,0,&i->state.inventory,RE7_INPUT_INVENTORY);
 if(i->ladder_address)byte(a,i->ladder_address,0,&i->state.ladder,RE7_INPUT_LADDER);
 int ok=get(a,base,0x8FB17F0,&pointer,8);status(i,RE7_INPUT_INTERACTION,ok);
 if(ok)i->state.interaction_pointer=pointer;
}
static void pointers(Access *a,uint64_t base) {
 Re7OriginalInputs *i=a->i;
 static const uint32_t po[]={0xD8,0x18},ao[]={0x20,0x88,0x10,0xA8};
 static const uint32_t io[]={0x208,0x58,0x148,0x30,0x48},ho[]={0xA0,0xE0,0x78,0x10,0x18,0xC0};
 if(!chain(a,base,0x8FB8018,po,2,0,&i->player_base))i->player_base=0;
 if(!chain(a,base,0x8FBB610,ao,4,0x20,&i->aim_address))i->aim_address=0;
 if(!chain(a,base,0x8FAC380,io,5,0x10,&i->inventory_address))i->inventory_address=0;
 if(!chain(a,base,0x8FBB610,ho,6,0,&i->head_address)){
  i->head_address=0;i->state.model_marker_available=0;status(i,RE7_INPUT_MODEL,0);
 }
 player(a);flags(a,base);
}
static void model_marker(Access *a,const Re7OriginalStateConfig *config) {
 Re7OriginalInputs *i=a->i;i->state.model_marker_available=0;
 if(!config||!config->autodetectmodel||!i->head_address)return;
 unsigned clancy=(1u<<RE7_ORIGINAL_CHAR_MAIN_CLANCY)|(1u<<RE7_ORIGINAL_CHAR_DLC_CLANCY);
 float value;int ok=scalar(a,i->head_address,(i->state.characters&clancy)?0x4BC:0x53C,&value);
 status(i,RE7_INPUT_MODEL,ok);
 if(ok){i->state.model_marker=value;i->state.model_marker_available=1;}
}
static uint32_t rounded(long double value) {
 int32_t result;
 __asm__("fistpl %0" : "=m"(result) : "t"(value) : "st");
 return (uint32_t)result;
}
static int between(uint32_t v,float low,float high) {return v>=bits(low)&&v<=bits(high);}
static void bodybag(Re7OriginalStateInput *s) {
 uint32_t x=bits(s->player.x),y=bits(s->player.y),z=bits(s->player.z);
 if(bits(s->autooldcamfov)<bits(74))return;
 s->bodybagarea=0;
 if((s->loadarea!=5&&s->loadarea!=24&&s->loadarea!=34)||s->moldgrabflag!=0)return;
 if(x>=bits(-1.95f)&&x<bits(-7.6f)&&between(y,-4,-6)&&between(z,12.85f,15.6f)){
  s->bodybagarea=1;return;
 }
 if(s->loadarea!=5)return;
 /* Source exit exclusion runs before the larger cage region. */
 if((x<bits(.4f)||(x>=0x80000000u&&x<bits(-1.7f)))&&
    between(y,-7.5f,-8)&&between(z,3.8f,5.1f))return;
 if((x<bits(.6f)||(x>=0x80000000u&&x<bits(-6.6f)))&&
    between(y,-7.5f,-8)&&between(z,3.8f,13.6f))s->bodybagarea=1;
}
static int mia_exception(const Re7OriginalStateInput *s) {
 if(s->loadarea!=4)return 0;
 uint32_t x=bits(s->player.x),y=bits(s->player.y),z=bits(s->player.z);
 if(between(x,-12.9f,-13.5f)&&
    (z<bits(10)||(z>=0x80000000u&&z<=bits(-1))))return 1;
 return between(x,-1.9f,-12.9f)&&
        (y<bits(1)||(y>=0x80000000u&&y<=bits(-1)))&&
        (z<bits(1.15f)||(z>=0x80000000u&&z<=bits(-.6f)));
}
static void timescale(Access *a,uint64_t base,const NativeFrame *f) {
 Re7OriginalInputs *i=a->i;Re7OriginalStateInput *s=&i->state;
 player(a);flags(a,base);
 float value;int ok=scalar(a,f->rbx,0x348,&value)&&value>=0;
 status(i,RE7_INPUT_TIMESCALE,ok);if(ok)i->timescale=value;
 uint64_t required=RE7_INPUT_PLAYER|RE7_INPUT_AIM|RE7_INPUT_INVENTORY|
  RE7_INPUT_INTERACTION|RE7_INPUT_FOV|RE7_INPUT_AREA|RE7_INPUT_FRAMERATE;
 if((i->valid&required)!=required){status(i,RE7_INPUT_SCENE,0);return;}
 if(s->firstpersonflag!=1&&bits(s->autooldcamfov)==bits(75)&&s->moldgrabflag!=1)s->recentfpflag=0;
 if((uint16_t)s->interaction_pointer!=0||s->inventory==1)
  s->inexitdelay=rounded((long double)s->framerate*(long double).3f);
 if(s->inexitdelay)s->inexitdelay--;
 bodybag(s); /* Must consume previous moldgrabflag, before this tick's refresh. */
 if(s->loadscreenflag==1){
  i->ladder_address=0;i->grab_address=0;s->ladder=0;
  status(i,RE7_INPUT_LADDER,1);status(i,RE7_INPUT_GRAB,1);
 }
 uint8_t grab=0;
 if(i->grab_address){
  ok=get(a,i->grab_address,0,&grab,1);status(i,RE7_INPUT_GRAB,ok);
  if(!ok){status(i,RE7_INPUT_SCENE,0);return;}
  if(grab==1)i->extendedgrabframes=rounded((long double)s->framerate/4);
  if(i->extendedgrabframes){
   if(bits(s->autooldcamfov)>=bits(74))i->extendedgrabframes--;
   grab=1;
  }
 }
 s->moldgrabflag=grab;
 if(mia_exception(s))s->moldgrabflag=0;
 status(i,RE7_INPUT_SCENE,1);
}
static void timer(Access *a,const NativeFrame *f) {
 Re7OriginalInputs *i=a->i;Re7OriginalStateInput *s=&i->state;
 if(i->frametrack>1000)i->frametrack=0;
 i->frametrack++;i->frame++;
 s->frametrack=i->frametrack;
 if(i->gbaimcheck>9)i->gbaimcheck=0;
 if(++i->gbaimcheck>3){s->aimblockedflag=0;status(i,RE7_INPUT_GUNBLOCK,1);}
 if(i->valid&RE7_INPUT_AIM){
  if(i->oldaim_address!=i->aim_address){
   s->loadscreenflag=1;s->igtimer=0;i->oldaim_address=i->aim_address;
   status(i,RE7_INPUT_TIMER,1);
  }
  if((i->valid&RE7_INPUT_TIMER)&&bits(s->igtimer)>=bits(1))s->loadscreenflag=0;
 }
 if(s->loadarea==35){
  float value;int ok=scalar(a,f->rbx,0x2D4,&value);
  status(i,RE7_INPUT_TIMER,ok);if(ok)s->igtimer=value;
 }
}
static void character(Access *a) {
 Re7OriginalInputs *i=a->i;uint32_t id;int ok=get(a,i->character_address,0,&id,4);
 unsigned ch=RE7_ORIGINAL_CHAR_ETHAN;
 if(ok){
  switch(id){
   case 1:ch=RE7_ORIGINAL_CHAR_CHRIS;break;
   case 2:ch=RE7_ORIGINAL_CHAR_PRESENT_MIA;break;
   case 3:{
    float z;ok=scalar(a,i->player_base,0x38,&z);
    if(ok)ch=(int32_t)bits(z)<(int32_t)bits(35)?RE7_ORIGINAL_CHAR_MAIN_CLANCY:RE7_ORIGINAL_CHAR_PRESENT_MIA;
    break;
   }
   case 4:ch=RE7_ORIGINAL_CHAR_MAIN_CLANCY;break;
   case 5:case 6:ch=RE7_ORIGINAL_CHAR_PAST_MIA;break;
   case 8:ch=RE7_ORIGINAL_CHAR_DLC_CLANCY;break;
   case 9:ch=RE7_ORIGINAL_CHAR_ZOE;break;
   default:if(i->state.loadarea==3)ch=RE7_ORIGINAL_CHAR_JOE;break;
  }
 }
 status(i,RE7_INPUT_CHARACTER,ok);
 if(ok){i->pcharcflag=id;i->state.characters=1u<<ch;}
}
static void remember(uint64_t *dst,uint64_t owner,uint32_t offset,size_t n) {
 if(!span(owner,offset,n,dst))*dst=0;
}
static void weapon(Access *a,const Re7OriginalStateConfig *config) {
 Re7OriginalInputs *i=a->i;uint32_t id;int ok=config&&get(a,i->weapon_address,0,&id,4);
 status(i,RE7_INPUT_WEAPON,ok);
 if(ok){
  unsigned kind=re7_original_state_weapon(config,id);i->weapon_raw=id;i->heldweaponid=(uint16_t)id;
  i->state.holdinggun=(kind==RE7_ORIGINAL_WEAPON_GUN);i->state.holdingmelee=(kind==RE7_ORIGINAL_WEAPON_MELEE);
 }
}
static void nogun(Access *a) {
 uint32_t u;int ok=get(a,a->i->nogun_address,0,&u,4);status(a->i,RE7_INPUT_NOGUN,ok);
 if(ok)a->i->state.nogunaccess=(u!=bits(1));
}
static void telescope(Access *a) {
 uint8_t raw;int ok=get(a,a->i->telescope_address,0,&raw,1);status(a->i,RE7_INPUT_TELESCOPE,ok);
 if(ok)a->i->state.telescopeflag=raw!=0;
}
static void guard(Re7OriginalInputs *i,float speed,int available) {
 int ok=available&&finite(speed);
 int candidate=ok&&i->guard_r9!=0&&bits(speed)==bits(.5f);
 if(candidate&&(i->valid&RE7_INPUT_WEAPON)==0)ok=0;
 candidate=candidate&&i->heldweaponid==1802;
 if(candidate&&(i->valid&RE7_INPUT_PLAYER)==0)ok=0;
 status(i,RE7_INPUT_GUARD,ok);
 if(ok)i->state.guardflag=candidate&&between(bits(i->state.player.y),-.59f,-.61f)&&
  between(bits(i->state.player.x),38.7f,54.8f)&&between(bits(i->state.player.z),21.4f,38.8f);
}
static void retry_failed(Access *a,const Re7OriginalStateConfig *config) {
 Re7OriginalInputs *i=a->i;
 /* Only retry actual failed event reads. No extra steady-state polling.
  * These are last observed producer-owned locations, not identity discovery. */
 if((i->failed&RE7_INPUT_WEAPON)&&i->weapon_address)weapon(a,config);
 if((i->failed&RE7_INPUT_CHARACTER)&&i->character_address)character(a);
 if((i->failed&RE7_INPUT_CROUCH)&&i->crouch_address)byte(a,i->crouch_address,0,&i->state.crouchflag,RE7_INPUT_CROUCH);
 if((i->failed&RE7_INPUT_NOGUN)&&i->nogun_address)nogun(a);
 if((i->failed&RE7_INPUT_TELESCOPE)&&i->telescope_address)telescope(a);
 if((i->failed&RE7_INPUT_GUARD)&&i->guard_address){
  float speed=0;int ok=scalar(a,i->guard_address,0,&speed);guard(i,speed,ok);
 }
}
static void ready(Re7OriginalInputs *i) {
 i->ready=(uint8_t)((i->valid&RE7_INPUT_REQUIRED)==RE7_INPUT_REQUIRED&&
  !(i->failed&~(uint64_t)RE7_INPUT_MODEL)&&i->state.player_available&&i->state.loadscreenflag!=1);
}
int re7_original_inputs_live_aim(Re7OriginalInputs *i,uint64_t base,
 Re7OriginalInputsRead read,void *ctx,uint8_t *value) {
 static const uint32_t offsets[]={0x20,0x88,0x10,0xA8};
 Access a={i,read,ctx};uint64_t address,confirmed;uint8_t next;
 if(!(i->valid&RE7_INPUT_AIM)||!i->aim_address||
    !chain(&a,base,0x8FBB610,offsets,4,0x20,&address)||address!=i->aim_address||
    !get(&a,address,0,&next,1)||
    !chain(&a,base,0x8FBB610,offsets,4,0x20,&confirmed)||confirmed!=address)return 0;
 *value=next;return 1;
}
void re7_original_inputs_init(Re7OriginalInputs *i) {
 if(!i)return;
 *i=(Re7OriginalInputs){0};
 i->state.framerate=60;i->state.flashlightflag=1;
 i->state.characters=1u<<RE7_ORIGINAL_CHAR_JOE;
 i->valid=RE7_INPUT_LADDER|RE7_INPUT_CHARACTER|RE7_INPUT_WEAPON|RE7_INPUT_CROUCH|
  RE7_INPUT_FRAMERATE|RE7_INPUT_TIMER|RE7_INPUT_NOGUN|RE7_INPUT_GRAB|
  RE7_INPUT_TELESCOPE|RE7_INPUT_GUARD|RE7_INPUT_FLASHLIGHT|RE7_INPUT_GUNBLOCK;
}
void re7_original_inputs_capture(Re7OriginalInputs *i,unsigned site,
 const NativeFrame *f,const void *fp,uint64_t base,
 Re7OriginalInputsRead read,void *ctx,const Re7OriginalStateConfig *config) {
 if(!i||site>=NATIVE_SITE_COUNT)return;
 i->observed|=UINT64_C(1)<<site;i->producer_calls[site]++;
 if(!f||!read){i->ready=0;return;}
 Access a={i,read,ctx};uint32_t u;uint64_t address;float value;int ok;
 switch(site){
  case NS_CUTSCENE_AUTOLOOK:
   pointers(&a,base);
   /* ID 3 needs player Z. Its event can precede pointer acquisition;
    * head-only mode has no policy/timescale retry sites. Retry only this
    * failed, previously observed input after its dependency is refreshed. */
   if((i->failed&RE7_INPUT_CHARACTER)&&i->character_address)character(&a);
   break;
  case NS_AFTER_ADAPTIVE_DIFF:flags(&a,base);retry_failed(&a,config);model_marker(&a,config);break;
  case NS_HEAD_Y_WRITE:
   if(config&&config->fpaimflag!=0)byte(&a,i->aim_address,0,&i->state.aim,RE7_INPUT_AIM);
   break;
  case NS_CAM_WRITES:
   /* Caller must filter exact camera identity before this hot-path capture. */
   byte(&a,i->aim_address,0,&i->state.aim,RE7_INPUT_AIM);break;
  case NS_CHARACTER_CMP:
   remember(&i->character_address,f->rdi,0x1E8,4);character(&a);break;
  case NS_LADDER_CMP:
   ok=span(f->rax,0x53,1,&address);status(i,RE7_INPUT_LADDER,ok);
   if(ok){i->ladder_address=address;i->state.ladder=1;}break;
  case NS_FOV_AUTOOLDCAM_WRITE:
   ok=scalar(&a,f->rax,0xB4,&value);status(i,RE7_INPUT_FOV,ok);
   if(ok)i->state.autooldcamfov=value;
   break;
  case NS_HELD_WEAPON_CMP:
   remember(&i->weapon_address,f->rbx,0x3B0,4);weapon(&a,config);break;
  case NS_GUN_BLOCK_DIS_CALC:case NS_NAH_GUN_BLOCK_DIS_CALC:
   ok=fp!=0;if(ok){value=native_get_xmm_scalar(fp,0);ok=finite(value);}
   status(i,RE7_INPUT_GUNBLOCK,ok);
   if(ok){i->gbaimcheck=0;i->state.aimblockedflag=bits(value)<=bits(.1f);}break;
  case NS_CROUCH_FLAG_READ:
   remember(&i->crouch_address,f->rbx,0x144,1);byte(&a,i->crouch_address,0,&i->state.crouchflag,RE7_INPUT_CROUCH);break;
  case NS_TIMESCALE_MULTIPLIER:timescale(&a,base,f);retry_failed(&a,config);break;
  case NS_LOADED_AREA:
   ok=get(&a,f->rax,0x54,&u,4);status(i,RE7_INPUT_AREA,ok);
   if(ok){
    if(i->igtupcheck>=3){i->igtpaused=1;i->igtupcheck=0;}
    if(i->updateigt==bits(i->state.igtimer))i->igtupcheck++;else i->igtpaused=0;
    i->updateigt=bits(i->state.igtimer);i->state.loadarea=u;
   }break;
  case NS_TIMESCALE_FRAMERATE:
   ok=fp!=0&&(i->valid&RE7_INPUT_TIMESCALE)!=0;
   if(ok){value=native_get_xmm_scalar(fp,0);ok=i->timescale==0||(finite(value)&&value>0);}
   status(i,RE7_INPUT_FRAMERATE,ok);
   if(ok)i->state.framerate=re7_original_collision_framerate(i->state.framerate,value,i->timescale);
   break;
  case NS_TIMER_AOB:timer(&a,f);break;
  case NS_BETTER_TIMER:
   ok=scalar(&a,f->rdi,0x50,&value);status(i,RE7_INPUT_TIMER,ok);if(ok)i->state.igtimer=value;break;
  case NS_NO_GUN_ACCESS_FLOAT:
   remember(&i->nogun_address,f->rbx,0xAC,4);nogun(&a);break;
  case NS_GRAB_FLAG_WHEN_ACTIVE_CMP:
   if(i->state.loadscreenflag!=1){
    ok=span(f->rdx,0xA8,1,&address);status(i,RE7_INPUT_GRAB,ok);if(ok)i->grab_address=address;
   }break;
  case NS_TELESCOPE_FLAG:
   remember(&i->telescope_address,f->rbx,0x60,1);telescope(&a);break;
  case NS_GUARD_FLOAT:
   remember(&i->guard_address,f->rbx,0x180,4);i->guard_r9=f->r9;
   guard(i,fp?native_get_xmm_scalar(fp,6):0,fp!=0);break;
  case NS_FLASHLIGHT_WRITE:
   ok=span(f->rcx,0x60,1,&address);status(i,RE7_INPUT_FLASHLIGHT,ok);
   if(ok){i->flashlight_address=address;i->state.flashlightflag=(uint8_t)f->rbp;}break;
  case NS_RUN_FLAG_OFF:i->state.runflag=0;break;
  case NS_RUN_FLAG_ON:i->state.runflag=1;break;
  default:break;
 }
 ready(i);
}
