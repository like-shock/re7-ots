/* Source-driven RT 8.02 L4007-4060, L9629-9759 adapter contracts.
 * These fixtures catch missing/wrong item production, effect branches,
 * decrement order, upper-lane corruption, stale state, and changed pointers. */
#include "native_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"line %d: %s\n",__LINE__,#c);exit(1);}}while(0)
#define M UINT64_C(0x100000)
#define BASE UINT64_C(0x140000000)
static unsigned char memory[0x10000],fp[1024],saved_fp[1024];
static NativeRuntime r;static NativeFrame f,saved;
static uint64_t bad_read,aimroot,replace_at,replace_value,replace_address,interaction_pointer;static int reads,writes,allow_write;
static void put(uint64_t a,const void*v,size_t n){CHECK(a>=M&&a-M+n<=sizeof memory);memcpy(memory+a-M,v,n);}
static void qword(uint64_t a,uint64_t v){put(a,&v,8);}
static void dword(uint64_t a,uint32_t v){put(a,&v,4);}
static void word(uint64_t a,uint16_t v){put(a,&v,2);}
static int rd(void*c,uint64_t a,void*out,size_t n){
 (void)c;reads++;if(a==bad_read)return 0;
 if(a==BASE+0x8fb17f0&&n==8){memcpy(out,&interaction_pointer,8);return 1;}
 if(a==BASE+0x8fbb610&&n==8){memcpy(out,&aimroot,8);return 1;}
 if(n==8&&(a==BASE+0x8fb8018||a==BASE+0x8fac380)){
  uint64_t root=a==BASE+0x8fb8018?M+0xa000:M+0xd000;memcpy(out,&root,8);return 1;
 }
 if(a<M||a-M>=sizeof memory||n>sizeof memory-(a-M))return 0;
 memcpy(out,memory+a-M,n);
 if(a==replace_at)qword(replace_address,replace_value);
 return 1;
}
static int wr(void*c,uint64_t a,const void*in,size_t n){
 (void)c;writes++;if(!allow_write||a<M||a-M>=sizeof memory||n>sizeof memory-(a-M))return 0;
 memcpy(memory+a-M,in,n);return 1;
}
int __attribute__((ms_abi)) native_dispatch(unsigned s,NativeFrame*g,void*p){(void)s;(void)g;(void)p;return 0;}
static unsigned site(const char*name){
 for(unsigned i=0;i<NATIVE_SITE_COUNT;i++)if(!strcmp(native_sites[i].name,name))return i;
 return NATIVE_SITE_COUNT; /* Missing sites still exercise the real runtime's replay result. */
}
static int call(unsigned s){return native_runtime_dispatch(&r,s,&f,fp);}
static void save(void){saved=f;memcpy(saved_fp,fp,sizeof fp);}
static void unchanged(void){CHECK(!memcmp(&f,&saved,sizeof f)&&!memcmp(fp,saved_fp,sizeof fp));CHECK(!writes);}
static void lanes(unsigned n,float a,float b,float c,float d){float v[]={a,b,c,d};memcpy(fp+160+16*n,v,16);}
static void setup(void){
 memset(memory,0xa5,sizeof memory);memset(fp,0x53,sizeof fp);memset(&f,0x72,sizeof f);
 bad_read=replace_at=UINT64_MAX;replace_value=interaction_pointer=0;replace_address=M+0x2078;reads=writes=allow_write=0;native_use_xsave=1;fp[512]&=~2;
 native_runtime_init(&r,BASE,rd,wr,0);
 r.inputs.valid=RE7_INPUT_REQUIRED;r.inputs.state.player_available=1;r.inputs.ready=1;
 r.inputs.state.loadarea=35;r.inputs.state.autooldcamfov=75;r.inputs.state.framerate=60;
 f.flags=0x202;
 aimroot=M+0x1000;qword(aimroot+0x20,M+0x1100);qword(M+0x1188,M+0x1200);
 qword(M+0x1210,M+0x1300);qword(M+0x13a8,M+0x1400);r.inputs.aim_address=M+0x1420;
 memory[0x1420]=1;r.inputs.inventory_address=M+0x1430;memory[0x1430]=0;
}
static void fallback_guards(unsigned s){
 setup();native_runtime_set_camera_enabled(&r,0);save();CHECK(!call(s));unchanged();CHECK(!reads);
 setup();r.inputs.state.loadscreenflag=1;save();CHECK(!call(s));unchanged();
 setup();r.inputs.valid&=~RE7_INPUT_PLAYER;save();CHECK(!call(s));unchanged();
}
static void item(uint16_t a,uint16_t b,uint16_t c){f.rbx=M+0x2000;qword(f.rbx+0x78,M+0x3000);word(M+0x3014,a);word(M+0x3026,b);word(M+0x3028,c);}
static void items(void){
 setup();item('N','2','2');save();CHECK(!call(NS_INVENTORY_HIGHLIGHTED_ITEM_ID));unchanged();
 CHECK(r.config.critterexamine==1&&r.interaction_input.scrapexamine==0);
 item('N','0','9');CHECK(!call(NS_INVENTORY_HIGHLIGHTED_ITEM_ID));CHECK(r.config.critterexamine==0&&r.interaction_input.scrapexamine==1);
 item('X','2','2');CHECK(!call(NS_INVENTORY_HIGHLIGHTED_ITEM_ID));CHECK(!r.config.critterexamine&&!r.interaction_input.scrapexamine);
 const uint64_t failures[]={M+0x2078,M+0x3014,M+0x3026,M+0x3028};
 for(unsigned j=0;j<4;j++){
  item('N','2','2');bad_read=UINT64_MAX;call(NS_INVENTORY_HIGHLIGHTED_ITEM_ID);CHECK(r.config.critterexamine==1);
  bad_read=failures[j];call(NS_INVENTORY_HIGHLIGHTED_ITEM_ID);CHECK(!r.config.critterexamine&&!r.interaction_input.scrapexamine);
 }
 bad_read=UINT64_MAX;item('N','2','2');replace_at=M+0x3028;replace_value=M+0x4000;
 call(NS_INVENTORY_HIGHLIGHTED_ITEM_ID);CHECK(!r.config.critterexamine&&!r.interaction_input.scrapexamine);
}
static void critter_policy(void){
 setup();item('N','2','2');call(NS_INVENTORY_HIGHLIGHTED_ITEM_ID);
 r.config.inventorydisable=0;r.config.autodetectmodel=0;r.config.fovcheckflag=0;r.config.loccheckflag=0;
 memset(r.config.autodisable_location,0,sizeof r.config.autodisable_location);
 memset(r.config.autodisable_character,0,sizeof r.config.autodisable_character);
 r.inputs.state.loadarea=37;r.inputs.state.interactableflag=1;memory[0x1430]=1;
 CHECK(!call(NS_AFTER_ADAPTIVE_DIFF));CHECK(r.state.stopflag==1);
 item('N','0','9');call(NS_INVENTORY_HIGHLIGHTED_ITEM_ID);
 CHECK(!call(NS_AFTER_ADAPTIVE_DIFF));CHECK(r.state.stopflag==0);
}
static void vignette(void){
 unsigned s=site("VIGNETTE");setup();save();CHECK(call(s));CHECK(f.rax==2);saved.rax=2;unchanged();
 setup();r.view.firstpersonflag=1;save();CHECK(!call(s));unchanged();fallback_guards(s);
}
static void timer(void){
 unsigned s=site("PSYCHO_TIMER"),close=site("CLOSE_DISTORTION");setup();f.rbx=M+0x2000;
 dword(f.rbx+0x240,0x3f400000);save();CHECK(call(s));
 float want[]={.75f,0,0,0};CHECK(!memcmp(fp+160,want,16));memcpy(saved_fp+160,want,16);saved_fp[512]|=2;unchanged();
 f.rcx=M+0x3000;qword(f.rcx+0x18,0x12345678abcdef01ULL);lanes(1,.125f,2,3,4);
 CHECK(call(close));CHECK(native_get_xmm_scalar(fp,1)==.25f);
 setup();f.rbx=M+0x2000;dword(f.rbx+0x240,0);CHECK(call(s));bad_read=f.rbx+0x240;save();CHECK(!call(s));unchanged();
 bad_read=UINT64_MAX;f.rcx=M+0x3000;qword(f.rcx+0x18,123);lanes(1,.125f,2,3,4);
 CHECK(call(close));CHECK(native_get_xmm_scalar(fp,1)==0);fallback_guards(s);
}
static void distortion(void){
 unsigned s=site("CLOSE_DISTORTION"),p=site("PSYCHO_TIMER");setup();f.rcx=M+0x3000;qword(f.rcx+0x18,0x12345678abcdef01ULL);
 lanes(1,.125f,2,3,4);save();CHECK(call(s));float zero[4]={0};
 CHECK(f.rax==0x12345678abcdef01ULL);saved.rax=f.rax;
 CHECK(!memcmp(fp+176,zero,16)&&!memcmp(fp+192,zero,16));memcpy(saved_fp+176,zero,16);memcpy(saved_fp+192,zero,16);saved_fp[512]|=2;unchanged();
 r.view.firstpersonflag=1;lanes(1,.125f,2,3,4);save();CHECK(!call(s));unchanged();
 for(int first=0;first<2;first++){
  setup();r.view.firstpersonflag=(uint8_t)first;f.rbx=M+0x2000;dword(f.rbx+0x240,0x3f400000);CHECK(call(p));
  f.rcx=M+0x3000;qword(f.rcx+0x18,0x87654321);lanes(1,-.06f,2,3,4);save();CHECK(call(s));
  float want[]={first?-.06f:-.12f,0,0,0};CHECK(!memcmp(fp+176,want,16)&&!memcmp(fp+192,want,16));
  memcpy(saved_fp+176,want,16);memcpy(saved_fp+192,want,16);saved_fp[512]|=2;saved.rax=0x87654321;unchanged();
  lanes(1,.125f,2,3,4);save();if(first){CHECK(!call(s));unchanged();}else{CHECK(call(s));CHECK(native_get_xmm_scalar(fp,1)==0);}
 }
 /* A failed original pointer read must replay an untouched frame and discard the stale pulse. */
 setup();f.rbx=M+0x2000;dword(f.rbx+0x240,0);CHECK(call(p));f.rcx=M+0x3000;bad_read=f.rcx+0x18;save();CHECK(!call(s));unchanged();
 bad_read=UINT64_MAX;qword(f.rcx+0x18,123);lanes(1,.125f,2,3,4);CHECK(call(s));CHECK(native_get_xmm_scalar(fp,1)==0);
 fallback_guards(s);
}
static void dot(void){
 unsigned s=site("NAH_DOT_FORCE");setup();f.r14=0x1234567800000010ULL;save();CHECK(call(s));CHECK(f.rbp==0&&f.rax==0xffffffdfu);saved.rbp=0;saved.rax=0xffffffdfu;unchanged();
 setup();r.view.firstpersonflag=1;save();CHECK(!call(s));unchanged();
 setup();r.inputs.state.loadarea=34;save();CHECK(!call(s));unchanged();
 setup();memory[0x1420]=0;save();CHECK(!call(s));unchanged();
 setup();r.inputs.state.aim=1;bad_read=r.inputs.aim_address;save();CHECK(!call(s));unchanged();
 setup();r.inputs.state.aim=1;r.inputs.aim_address=0;save();CHECK(!call(s));unchanged();
 setup();r.inputs.state.aim=1;aimroot=M+0x4000;save();CHECK(!call(s));unchanged();
 setup();qword(M+0x13a8,M+0x1500);memory[0x1520]=1;save();CHECK(!call(s));unchanged(); /* old address must not grant control */
 r.inputs.aim_address=M+0x1520;CHECK(call(s)); /* producer reacquisition */
 setup();replace_at=r.inputs.aim_address;replace_address=M+0x13a8;replace_value=M+0x1500;
 save();CHECK(!call(s));unchanged(); /* replacement while reading the byte */
 fallback_guards(s);
}
static void redline(void){
 unsigned s=site("NAH_CRIT_HP_EFFECT_CMP");setup();f.flags=0xffffffffffffffffULL;save();CHECK(call(s));CHECK(f.flags==0xfffffffffffff76eULL);saved.flags=f.flags;unchanged();
 /* No area gate: source TP predicate alone, even if a matching call arrives elsewhere. */
 r.inputs.state.loadarea=37;CHECK(call(s));
 r.view.firstpersonflag=1;save();CHECK(!call(s));unchanged();fallback_guards(s);
}
static void reset(void){
 unsigned p=site("PSYCHO_TIMER"),s=site("CLOSE_DISTORTION");
 for(int mode=0;mode<4;mode++){
  setup();item('N','2','2');call(NS_INVENTORY_HIGHLIGHTED_ITEM_ID);f.rbx=M+0x2000;dword(f.rbx+0x240,0);CHECK(call(p));
  if(mode==0){native_runtime_set_camera_enabled(&r,0);native_runtime_set_camera_enabled(&r,1);}
  if(mode==1){r.inputs.state.loadscreenflag=1;call(site("VIGNETTE"));}
  if(mode==2){r.inputs.valid&=~RE7_INPUT_PLAYER;call(site("VIGNETTE"));}
  if(mode==3){dword(f.rbx+0x2d4,0);CHECK(!call(NS_TIMER_AOB));CHECK(r.inputs.state.loadscreenflag==1);}
  CHECK(!r.config.critterexamine&&!r.interaction_input.scrapexamine);
  r.inputs.state.loadscreenflag=0;r.inputs.valid|=RE7_INPUT_PLAYER;r.inputs.failed=0;r.view.firstpersonflag=0;
  f.rcx=M+0x3000;qword(f.rcx+0x18,123);lanes(1,.125f,2,3,4);CHECK(call(s));CHECK(native_get_xmm_scalar(fp,1)==0);
 }
}

/* HP and head chains share the source root, but have different leaves.
 * Keep all basic producers valid so every test enters the real adapter. */
static void player_chains(void){
 qword(aimroot+0xa0,M+0x2000);qword(M+0x20d0,M+0x3000);qword(M+0x3070,M+0x4000);
 qword(M+0x20e0,M+0x5000);qword(M+0x5078,M+0x6000);qword(M+0x6010,M+0x7000);
 qword(M+0x7018,M+0x8000);qword(M+0x80c0,M+0x9000);dword(M+0x953c,0x3f800000);
 qword(M+0xa0d8,M+0xb000);qword(M+0xb018,M+0xc000);dword(M+0xc030,0);dword(M+0xc034,0);dword(M+0xc038,0);
 qword(M+0xd208,M+0xd400);qword(M+0xd458,M+0xd800);qword(M+0xd948,M+0xdc00);
 qword(M+0xdc30,M+0xe000);qword(M+0xe048,M+0xe400);memory[0xe410]=0;
 r.inputs.valid|=RE7_INPUT_CHARACTER|RE7_INPUT_FRAMERATE;r.inputs.state.characters=1u<<RE7_ORIGINAL_CHAR_ETHAN;
}
static void observe(void){CHECK(!call(NS_CUTSCENE_AUTOLOOK));CHECK(r.inputs.ready);}
static void tick(void){f.rbx=M+0xf000;dword(f.rbx+0x348,0x3f800000);CHECK(!call(NS_TIMESCALE_MULTIPLIER));CHECK(r.inputs.ready);}
static void blood(void){
 unsigned s=site("SCREEN_BLOOD_WRITE_FOLLOW");
 /* FP cutoff is HP/10 rounded to single, compared as unsigned float bits;
  * negative/NaN values follow those exact source bits, not float ordering. */
 const struct {uint32_t hp;int first,hide;} cases[]={
  {0,0,1},{0x447a0000,0,1},{0,1,0},{0x4479ffff,1,0},{0x447a0000,1,1},
  {0x447a0001,1,1},{0x80000000,1,1},{0x7f800000,1,1},{0x7fc12345,1,1}};
 for(unsigned j=0;j<sizeof cases/sizeof cases[0];j++){
  setup();player_chains();observe();r.view.firstpersonflag=(uint8_t)cases[j].first;
  dword(M+0x4014,cases[j].hp);f.rdi=M+0xf000;dword(f.rdi+0x48,0x3f400000);save();
  if(!cases[j].hide){CHECK(!call(s));unchanged();continue;}
  CHECK(call(s));float zero[4]={0};CHECK(!memcmp(fp+160,zero,16));
  memcpy(saved_fp+160,zero,16);saved_fp[512]|=2;unchanged();
 }
 /* Puzzle cancellation precedes missing HP, and does not read engine blood. */
 setup();r.state.stoppedbloodhide=1;f.rdi=UINT64_MAX;save();CHECK(call(s));
 float zero[4]={0};memcpy(saved_fp+160,zero,16);saved_fp[512]|=2;unchanged();
 fallback_guards(s);
}
static void blood_identity(void){
 unsigned s=site("SCREEN_BLOOD_WRITE_FOLLOW");setup();player_chains();f.rdi=M+0xf000;
 save();CHECK(!call(s));unchanged(); /* no producer */
 observe();dword(M+0x4014,0x447a0000);bad_read=M+0x4014;save();CHECK(!call(s));unchanged();
 bad_read=UINT64_MAX;observe();qword(M+0x3070,M+0x4100);dword(M+0x4114,0x447a0000);
 save();CHECK(!call(s));unchanged(); /* replacement requires producer reacquisition */
 observe();CHECK(call(s));
 setup();player_chains();observe();replace_at=M+0x4014;replace_address=M+0x3070;replace_value=M+0x4100;
 save();CHECK(!call(s));unchanged(); /* replacement during value read */
 setup();player_chains();observe();bad_read=M+0x20d0;save();CHECK(!call(s));unchanged();
 setup();player_chains();observe();qword(M+0x3070,UINT64_MAX-8);save();CHECK(!call(s));unchanged();
}
static void hurt(void){
 unsigned s=site("HURT_FILTER_READ");const uint32_t input[]={0,1,2,UINT32_MAX,0x80000000};
 for(unsigned first=0;first<2;first++)for(unsigned j=0;j<sizeof input/sizeof input[0];j++){
  setup();r.view.firstpersonflag=(uint8_t)first;f.rsi=M+0xf000;dword(f.rsi+0x18,input[j]);
  f.rdi=UINT64_MAX;f.flags=UINT64_MAX;save();CHECK(call(s));
  uint32_t want=input[j]==0?(first?0:2):input[j];CHECK(f.r8==want&&f.flags==UINT64_C(0xfffffffffffff72a));
  saved.r8=want;saved.flags=f.flags;unchanged(); /* entire RAX/FP frame preserved */
 }
 setup();f.rsi=M+0xf000;bad_read=f.rsi+0x18;save();CHECK(!call(s));unchanged();fallback_guards(s);
}
static void head_delay(void){
 setup();player_chains();allow_write=1;r.view.firstpersonflag=1;r.state.stopflag=1;observe();
 r.state.stoppedbloodhide=1;tick();tick(); /* repeated stopped frames refresh to 2 */
 r.state.stoppedbloodhide=0;r.state.stopflag=0;f.rbx=M+0x9000;
 CHECK(call(NS_HEAD_Y_WRITE));CHECK(r.view.firstpersonflag==0);CHECK(r.head.head==M+0x9000);
 for(unsigned j=0;j<3;j++)CHECK(call(NS_HEAD_Y_WRITE)); /* not a transform-call countdown */
 tick();observe();f.rbx=M+0x9000;CHECK(call(NS_HEAD_Y_WRITE)); /* producer retains TP delay cache */
 tick();f.rbx=M+0x9000;writes=0;CHECK(!call(NS_HEAD_Y_WRITE));CHECK(!writes&&!r.head.head);
}
static void head_delay_invalidations(void){
 for(unsigned mode=0;mode<5;mode++){
  setup();player_chains();allow_write=1;r.view.firstpersonflag=1;r.state.stopflag=1;observe();
  r.state.stoppedbloodhide=1;tick();r.state.stoppedbloodhide=0;r.state.stopflag=0;
  if(mode==0)qword(M+0x80c0,M+0x9100);
  if(mode==1)r.inputs.state.characters=1u<<RE7_ORIGINAL_CHAR_PRESENT_MIA;
  if(mode==2){r.inputs.state.loadscreenflag=1;call(site("VIGNETTE"));r.inputs.state.loadscreenflag=0;}
  if(mode==3){native_runtime_set_camera_enabled(&r,0);native_runtime_set_camera_enabled(&r,1);}
  if(mode==4){bad_read=M+0x953c;}
  f.rbx=M+0x9000;writes=0;save();CHECK(!call(NS_HEAD_Y_WRITE));unchanged();
 }
 /* Loading discards cached head targets even if the next transform is FP. */
 setup();player_chains();r.view.firstpersonflag=1;r.state.stopflag=1;observe();
 r.inputs.state.loadscreenflag=1;call(site("VIGNETTE"));CHECK(!r.head.head);
}


static void hurt_status(void){
 unsigned s=site("HURT_FILTER_READ");setup();f.rsi=M+0xf000;dword(f.rsi+0x18,0);
 CHECK(call(s));CHECK(r.screen.attemptedfilter==98&&r.screen.hurtfilterframes==3);
 dword(f.rsi+0x18,7);CHECK(call(s));CHECK(r.screen.attemptedfilter==98&&r.screen.hurtfilterframes==2);
 dword(f.rsi+0x18,UINT32_MAX);CHECK(call(s));CHECK(r.screen.attemptedfilter==98&&r.screen.hurtfilterframes==1);
 CHECK(call(s));CHECK(r.screen.attemptedfilter==98&&!r.screen.hurtfilterframes);
 dword(f.rsi+0x18,9);CHECK(call(s));CHECK(r.screen.attemptedfilter==9&&!r.screen.hurtfilterframes);
 dword(f.rsi+0x18,0);CHECK(call(s));CHECK(r.screen.hurtfilterframes==3);
 bad_read=f.rsi+0x18;save();CHECK(!call(s));unchanged();CHECK(!r.screen.hurtfilterframes&&!r.screen.attemptedfilter);
}
static void blood_reset(void){
 unsigned s=site("SCREEN_BLOOD_WRITE_FOLLOW");
 for(unsigned mode=0;mode<3;mode++){
  setup();player_chains();observe();r.state.stoppedbloodhide=1;tick();CHECK(call(s));
  if(mode==0){native_runtime_set_camera_enabled(&r,0);native_runtime_set_camera_enabled(&r,1);}
  if(mode==1){r.inputs.state.loadscreenflag=1;call(site("VIGNETTE"));r.inputs.state.loadscreenflag=0;}
  if(mode==2){r.inputs.valid&=~RE7_INPUT_PLAYER;call(site("VIGNETTE"));r.inputs.valid|=RE7_INPUT_PLAYER;}
  save();CHECK(!call(s));unchanged();CHECK(!r.screen.hp_address&&!r.screen.stoppedbloodheaddelay);
 }
}

int main(int argc,char**argv){
 CHECK(argc==2);
 if(!strcmp(argv[1],"items"))items();else if(!strcmp(argv[1],"policy"))critter_policy();
 else if(!strcmp(argv[1],"vignette"))vignette();else if(!strcmp(argv[1],"timer"))timer();
 else if(!strcmp(argv[1],"distortion"))distortion();else if(!strcmp(argv[1],"dot"))dot();
 else if(!strcmp(argv[1],"blood"))blood();else if(!strcmp(argv[1],"blood_identity"))blood_identity();
 else if(!strcmp(argv[1],"hurt_status"))hurt_status();else if(!strcmp(argv[1],"blood_reset"))blood_reset();
 else if(!strcmp(argv[1],"hurt"))hurt();else if(!strcmp(argv[1],"head_delay"))head_delay();
 else if(!strcmp(argv[1],"head_invalid"))head_delay_invalidations();
 else if(!strcmp(argv[1],"redline"))redline();else if(!strcmp(argv[1],"reset"))reset();else return 2;
 puts("native screen contract passed");return 0;
}
