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
static uint64_t bad_read,aimroot,replace_at,replace_value,replace_address,interaction_pointer;static int reads,writes;
static void put(uint64_t a,const void*v,size_t n){CHECK(a>=M&&a-M+n<=sizeof memory);memcpy(memory+a-M,v,n);}
static void qword(uint64_t a,uint64_t v){put(a,&v,8);}
static void dword(uint64_t a,uint32_t v){put(a,&v,4);}
static void word(uint64_t a,uint16_t v){put(a,&v,2);}
static int rd(void*c,uint64_t a,void*out,size_t n){
 (void)c;reads++;if(a==bad_read)return 0;
 if(a==BASE+0x8fb17f0&&n==8){memcpy(out,&interaction_pointer,8);return 1;}
 if(a==BASE+0x8fbb610&&n==8){memcpy(out,&aimroot,8);return 1;}
 if(a<M||a-M>=sizeof memory||n>sizeof memory-(a-M))return 0;
 memcpy(out,memory+a-M,n);
 if(a==replace_at)qword(replace_address,replace_value);
 return 1;
}
static int wr(void*c,uint64_t a,const void*in,size_t n){(void)c;(void)a;(void)in;(void)n;writes++;return 0;}
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
 bad_read=replace_at=UINT64_MAX;replace_value=interaction_pointer=0;replace_address=M+0x2078;reads=writes=0;native_use_xsave=1;fp[512]&=~2;
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
int main(int argc,char**argv){
 CHECK(argc==2);
 if(!strcmp(argv[1],"items"))items();else if(!strcmp(argv[1],"policy"))critter_policy();
 else if(!strcmp(argv[1],"vignette"))vignette();else if(!strcmp(argv[1],"timer"))timer();
 else if(!strcmp(argv[1],"distortion"))distortion();else if(!strcmp(argv[1],"dot"))dot();
 else if(!strcmp(argv[1],"redline"))redline();else if(!strcmp(argv[1],"reset"))reset();else return 2;
 puts("native screen contract passed");return 0;
}
