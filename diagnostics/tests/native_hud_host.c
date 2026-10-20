/* CT9274-9627,10229-10244: observable bytes and full frames, not policy copies.
 * The sparse game-memory boundary is the only fake; all adapters are real. */
#include "native_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"line %d: %s\n",__LINE__,#c);exit(1);}}while(0)
#define M UINT64_C(0x100000)
#define BASE UINT64_C(0x140000000)
#define DISPLAY (M+0x1000)
#define AMMO (M+0x356c)
#define HALF (M+0x426c)
#define TALK (M+0x5628)
#define HUD (M+0x6020)
#define STACK (M+0x7000)
static unsigned char memory[0x10000],old_memory[0x10000],fp[1024],saved_fp[1024];
/* Full completed bridge allocation: 128 mutable +160 XMM +64 GPR +40 machine. */
static union {uint64_t align;unsigned char bytes[392];} backing;
#define f (*(NativeFrame*)backing.bytes)
static NativeFrame saved;static NativeRuntime r;
static uint64_t roots[3],bad_read,replace_at,replace_address,replace_value;
static unsigned writes,fail_write;static int partial,fail_rollback;
static void put(uint64_t a,const void*v,size_t n){CHECK(a>=M&&a-M+n<=sizeof memory);memcpy(memory+a-M,v,n);}
static void qword(uint64_t a,uint64_t v){put(a,&v,8);}
static void dword(uint64_t a,uint32_t v){put(a,&v,4);}
static void byte(uint64_t a,uint8_t v){put(a,&v,1);}
static uint8_t get(uint64_t a){return memory[a-M];}
static int rd(void*c,uint64_t a,void*out,size_t n){
 (void)c;if(a==bad_read)return 0;
 const uint64_t globals[]={BASE+0x8fb5778,BASE+0x8fb0ac8,BASE+0x8ff0c18};
 for(unsigned j=0;j<3;j++)if(a==globals[j]&&n==8){memcpy(out,&roots[j],8);return 1;}
 if(a<M||a-M>=sizeof memory||n>sizeof memory-(a-M))return 0;
 memcpy(out,memory+a-M,n);
 if(a==replace_at){qword(replace_address,replace_value);replace_at=UINT64_MAX;}
 return 1;
}
static int wr(void*c,uint64_t a,const void*in,size_t n){
 (void)c;writes++;if(a<M||a-M>=sizeof memory||n>sizeof memory-(a-M))return 0;
 if(writes==fail_write||(fail_rollback&&writes>fail_write)){
  if(partial)memcpy(memory+a-M,in,n);
  return 0;
 }
 memcpy(memory+a-M,in,n);return 1;
}
int __attribute__((ms_abi)) native_dispatch(unsigned s,NativeFrame*g,void*p){(void)s;(void)g;(void)p;return 0;}
static unsigned site(const char*name){
 for(unsigned j=0;j<NATIVE_SITE_COUNT;j++)if(!strcmp(native_sites[j].name,name))return j;
 return NATIVE_SITE_COUNT;
}
static int call(const char*name){return native_runtime_dispatch(&r,site(name),&f,fp);}
static void save(void){saved=f;memcpy(saved_fp,fp,sizeof fp);memcpy(old_memory,memory,sizeof memory);writes=0;}
static void unchanged(void){CHECK(!memcmp(&f,&saved,sizeof f));CHECK(!memcmp(fp,saved_fp,sizeof fp));}
static void ready(void){r.inputs.valid=RE7_INPUT_REQUIRED|RE7_INPUT_WEAPON|RE7_INPUT_TIMER;r.inputs.state.player_available=1;r.inputs.ready=1;r.inputs.state.loadscreenflag=0;}
static void setup(void){
 memset(memory,0,sizeof memory);memset(fp,0x53,sizeof fp);memset(&backing,0x72,sizeof backing);
 bad_read=replace_at=UINT64_MAX;writes=fail_write=partial=fail_rollback=0;
 native_runtime_init(&r,BASE,rd,wr,0);ready();native_use_xsave=1;fp[512]&=~2;
 r.inputs.state.loadarea=35;r.inputs.heldweaponid=100;r.inputs.state.igtimer=20;
 roots[0]=M+0x3000;qword(M+0x3028,M+0x3100);qword(M+0x33b0,M+0x3200);
 qword(M+0x3210,M+0x3300);qword(M+0x3448,M+0x3400);qword(M+0x3410,M+0x3500);
 roots[1]=M+0x4000;qword(M+0x4058,M+0x4100);qword(M+0x4110,M+0x4200);
 roots[2]=M+0x5000;qword(M+0x5040,M+0x5100);qword(M+0x5448,M+0x5200);qword(M+0x5250,M+0x5300);
 f.rbx=DISPLAY;f.rdi=HUD-0x20;f.r12=0x1234567800000001ULL;f.flags=0xed7;
 uint64_t rsp=STACK;memcpy(backing.bytes+376,&rsp,8);qword(STACK+0x38,0xabcdef9876543210ULL);
 dword(DISPLAY+0x18,0x42c80000);byte(HUD,9);byte(HALF,7);
}
static void observe(void){CHECK(call("NAH_OXYGEN_REF"));}
static void hud(uint8_t want,uint8_t half){
 f.r12=0x1234567800000001ULL;save();CHECK(call("NAH_HUD_WRITE"));
 CHECK(get(HUD)==want&&get(HALF)==half);CHECK(f.r12==0xabcdef9876543210ULL);
 saved.r12=f.r12;unchanged();
}
static void pos(float x,float z){r.inputs.state.player.x=x;r.inputs.state.player.z=z;}
static void talk(uint8_t value){byte(TALK,value);}
static void producer(void){
 setup();save();observe();CHECK(native_get_xmm_scalar(fp,2)==100.f);
 const float want[]={100,0,0,0};CHECK(!memcmp(fp+192,want,16));CHECK(!memcmp(&f,&saved,sizeof f));CHECK(!writes);CHECK(fp[512]&2);
 setup();bad_read=DISPLAY+0x18;save();CHECK(!call("NAH_OXYGEN_REF"));unchanged();CHECK(!writes);
}
static void hidden(void){setup();observe();hud(0,1);}
static void oxygen(void){
 const uint32_t samples[]={0x42c60000,0x42c80000,0x42ca0000,0x7fc00000,0x80000000};
 const uint8_t want[]={1,0,1,1,1};
 for(unsigned j=0;j<5;j++){setup();dword(DISPLAY+0x18,samples[j]);observe();hud(want[j],1);
  f.rbx=DISPLAY;f.r14=0x123456789;dword(DISPLAY+0x170,0x11223344);save();
  CHECK(call("NAH_OXYGEN_HUD_SWITCH"));unchanged();CHECK(!writes&&get(DISPLAY+0x170)==0x44);
  r.view.firstpersonflag=1;save();CHECK(!call("NAH_OXYGEN_HUD_SWITCH"));unchanged();CHECK(!writes);
 }
}
static void guards(void){
 setup();r.inputs.state.loadarea=34;save();CHECK(!call("NAH_HUD_WRITE"));unchanged();CHECK(!writes);
 setup();observe();r.view.firstpersonflag=1;hud(1,1);
 r.state.stoppedbloodhide=1;hud(0,1);
 setup();observe();r.inputs.valid&=~RE7_INPUT_PLAYER;save();CHECK(!call("NAH_HUD_WRITE"));unchanged();CHECK(!writes);
}
static void empty(void){
 const uint32_t ids[]={0,99,100,UINT32_MAX};
 for(unsigned j=0;j<4;j++){setup();observe();byte(AMMO,1);r.inputs.heldweaponid=ids[j];hud(j<2?0:1,j<2?1:0);CHECK(get(AMMO)==(j<2?0:1));}
}
static void ammo(void){
 setup();observe();hud(0,1);byte(AMMO,1);hud(0,1);hud(1,0);hud(1,0);
 byte(AMMO,0);hud(0,1);byte(AMMO,1);hud(0,1);hud(1,0);
}
static void notices(void){
 setup();observe();byte(DISPLAY+0x120,1);hud(0,1);CHECK(get(DISPLAY+0x128)==1);
 byte(DISPLAY+0x120,0);hud(0,1);CHECK(get(DISPLAY+0x128)==0);
 const unsigned offsets[]={0x174,0x198};
 for(unsigned j=0;j<2;j++){byte(DISPLAY+offsets[j],2);hud(1,1);byte(DISPLAY+offsets[j],0);hud(0,1);
  byte(DISPLAY+offsets[j]+1,1);hud(0,1);byte(DISPLAY+offsets[j]+1,0);}
 r.view.firstpersonflag=1;byte(DISPLAY+0x128,7);hud(1,1);CHECK(get(DISPLAY+0x128)==7);
}
static void shaft(void){
 const float coordinates[][2]={{-10,-75},{-20,-90},{-15,-80},{-9.99f,-80},{-20.01f,-80},{-15,-74.99f},{-15,-90.01f},{15,-80}};
 const uint8_t want[]={1,1,1,0,0,0,0,0};
 for(unsigned j=0;j<8;j++){setup();observe();pos(coordinates[j][0],coordinates[j][1]);r.inputs.state.igtimer=11.99f;hud(want[j],1);}
 setup();observe();pos(-15,-80);r.inputs.state.igtimer=12;hud(0,1);talk(1);hud(1,1);talk(0);hud(0,1);
}
static void bomb(void){
 setup();observe();pos(105,-225);talk(1);hud(0,1);hud(0,1);talk(0);hud(0,1);
 talk(1);hud(1,1);hud(1,1);talk(0);hud(0,1);talk(1);hud(0,1);
 /* Canister region reuses bombtalk; return to bomb permits another first talk. */
 pos(67,-185);talk(0);hud(0,1);pos(105,-225);talk(1);hud(0,1);talk(0);hud(0,1);talk(1);hud(1,1);
}
static void mainroom(void){
 setup();observe();pos(67,-185);talk(1);hud(1,1);pos(0,0);hud(1,1);talk(0);hud(0,1);
 pos(67,-180);talk(1);hud(0,1);pos(0,0);hud(0,1);talk(0);hud(0,1);
 pos(67,-185);talk(1);hud(1,1);
}
static void freeze(void){
 setup();observe();pos(45,-100);talk(1);hud(0,1);hud(0,1);talk(0);hud(0,1);talk(1);hud(1,1);
}
static void gauntlet(void){
 setup();observe();pos(90,-180);
 for(unsigned j=0;j<3;j++){talk(1);hud(0,1);hud(0,1);talk(0);hud(0,1);}
 talk(1);hud(1,1);
}
static void loading(void){
 const float xs[]={45,90,105};const float zs[]={-100,-180,-225};
 for(unsigned j=0;j<3;j++){
  setup();observe();pos(xs[j],zs[j]);talk(1);hud(0,1);
  r.inputs.state.loadscreenflag=1;save();CHECK(!call("NAH_HUD_WRITE"));unchanged();CHECK(!writes);
  save();CHECK(!call("NAH_OXYGEN_REF"));CHECK(!call("NAH_HUD_WRITE"));unchanged();CHECK(!writes);
  ready();observe();hud(j<2?1:0,1);
 }
 setup();observe();pos(45,-100);r.inputs.valid&=~RE7_INPUT_PLAYER;r.inputs.state.loadscreenflag=1;
 CHECK(!call("NAH_HUD_WRITE"));ready();observe();talk(1);hud(0,1);
}
static void identity(void){
 const uint64_t missing[]={BASE+0x8fb5778,M+0x3028,M+0x33b0,M+0x3210,M+0x3448,M+0x3410,AMMO,
  BASE+0x8fb0ac8,M+0x4058,M+0x4110,HALF,BASE+0x8ff0c18,M+0x5040,M+0x5448,M+0x5250,TALK,
  DISPLAY+0x18,DISPLAY+0x120,DISPLAY+0x174,DISPLAY+0x198,DISPLAY+0x128,STACK+0x38,HUD};
 for(unsigned j=0;j<sizeof missing/sizeof missing[0];j++){
  setup();observe();bad_read=missing[j];save();CHECK(!call("NAH_HUD_WRITE"));unchanged();CHECK(!writes);
 }
 setup();observe();replace_at=TALK;replace_address=M+0x3028;replace_value=M+0x8000;save();
 CHECK(!call("NAH_HUD_WRITE"));unchanged();CHECK(!writes);
 setup();observe();f.rbx=DISPLAY+0x400;save();CHECK(!call("NAH_OXYGEN_HUD_SWITCH"));unchanged();CHECK(!writes);
 setup();observe();hud(0,1);f.rbx=DISPLAY+0x400;dword(f.rbx+0x18,0x42c80000);observe();byte(AMMO,1);hud(1,0);
 setup();observe();dword(DISPLAY+0x18,0x42c60000);save();CHECK(!call("NAH_HUD_WRITE"));unchanged();CHECK(!writes);
}
static void failures(void){
 for(unsigned j=1;j<=4;j++){
  setup();observe();r.inputs.heldweaponid=99;byte(AMMO,1);byte(DISPLAY+0x120,1);save();fail_write=j;partial=1;
  CHECK(!call("NAH_HUD_WRITE"));unchanged();CHECK(!memcmp(memory,old_memory,sizeof memory));
  fail_write=0;save();CHECK(!call("NAH_HUD_WRITE"));unchanged();CHECK(!writes);observe();hud(0,1);
 }
 setup();observe();save();fail_write=2;fail_rollback=partial=1;CHECK(!call("NAH_HUD_WRITE"));unchanged();
 fail_write=fail_rollback=0;save();CHECK(!call("NAH_HUD_WRITE"));unchanged();CHECK(!writes);
}
static void reset(void){
 for(unsigned j=0;j<3;j++){
  setup();observe();pos(105,-225);talk(1);hud(0,1);talk(0);hud(0,1);talk(1);hud(1,1);
  if(j==0){native_runtime_set_camera_enabled(&r,0);save();CHECK(!call("NAH_HUD_WRITE"));unchanged();CHECK(!writes);native_runtime_set_camera_enabled(&r,1);r.view.firstpersonflag=0;}
  if(j==1){r.inputs.state.loadarea=34;CHECK(!call("NAH_HUD_WRITE"));r.inputs.state.loadarea=35;}
  if(j==2){r.inputs.valid&=~RE7_INPUT_PLAYER;CHECK(!call("NAH_HUD_WRITE"));ready();}
  save();CHECK(!call("NAH_HUD_WRITE"));unchanged();CHECK(!writes);observe();hud(0,1);
 }
}
static void alias(void){
 setup();observe();f.rdi=STACK+0x38-0x20;save();CHECK(!call("NAH_HUD_WRITE"));unchanged();CHECK(!writes);
 setup();observe();qword(M+0x4110,STACK+0x38-0x6c);save();CHECK(!call("NAH_HUD_WRITE"));unchanged();CHECK(!writes);
 setup();observe();qword(M+0x4110,M+0x3028-0x6c);save();CHECK(!call("NAH_HUD_WRITE"));unchanged();CHECK(!writes);
 setup();observe();qword(M+0x4110,AMMO-0x6c);save();CHECK(!call("NAH_HUD_WRITE"));unchanged();CHECK(!writes);
}
static void transmission(void){
 setup();observe();r.view.firstpersonflag=1;talk(1);hud(1,1);
 setup();observe();pos(-15,-80);r.inputs.state.igtimer=11;hud(1,1);
 pos(0,0);talk(1);hud(1,1);talk(2);hud(1,1);talk(0);hud(0,1);
}
static void owners(void){
 const uint64_t slots[]={M+0x3410,M+0x4110,M+0x5250};
 const uint64_t targets[]={M+0x3600,M+0x4300,M+0x5700};
 for(unsigned j=0;j<3;j++){
  setup();observe();hud(0,1);qword(slots[j],targets[j]);save();
  CHECK(!call("NAH_HUD_WRITE"));unchanged();CHECK(!writes);
  observe();f.r12=1;CHECK(call("NAH_HUD_WRITE"));CHECK(get(HUD)==0);
 }
}
/* Reproduce replacement after a forward store or while reversing a partial
 * write. Retired HALF stays writable: an unchecked restoration would succeed
 * against the wrong owner rather than being hidden by a fake memory failure. */
static unsigned retire_on_write,rollback_stale_writes;
static int rollback_retired,retire_kind;
static int retire_owner_during_write(void*c,uint64_t a,const void*in,size_t n){
 if(rollback_retired&&a==HALF)rollback_stale_writes++;
 int result=wr(c,a,in,n);
 if(writes==retire_on_write){
  if(retire_kind==0)qword(M+0x4110,M+0x4300);
  if(retire_kind==1)bad_read=M+0x4110;
  if(retire_kind==2)dword(DISPLAY+0x18,0x42c60000);
  rollback_retired=1;
 }
 return result;
}
static void rollback_case(unsigned replacement_write,unsigned failure){
 for(retire_kind=0;retire_kind<3;retire_kind++){
  setup();observe();byte(DISPLAY+0x120,1);byte(M+0x436c,9);save();
  rollback_retired=rollback_stale_writes=0;retire_on_write=replacement_write;fail_write=failure;partial=1;
  r.write=retire_owner_during_write;
  CHECK(!call("NAH_HUD_WRITE"));unchanged();CHECK(!rollback_stale_writes);
  CHECK(writes==replacement_write);CHECK(get(HALF)==1&&get(M+0x436c)==9);
  CHECK(!r.hud.display&&!r.hud.have_owners&&!r.hud.nahhudhidden&&!r.hud.nahtrans);
  save();CHECK(!call("NAH_HUD_WRITE"));unchanged();CHECK(!writes);
 }
}
static void rollback_replaced(void){rollback_case(1,0);}
static void rollback_failed_write(void){rollback_case(1,1);}
static void rollback_replaced_during_restore(void){rollback_case(3,2);}
int main(int argc,char**argv){
 CHECK(argc==2);
#define CASE(name) if(!strcmp(argv[1],#name)){name();puts("NAH HUD " #name ": PASS");return 0;}
 CASE(producer) CASE(hidden) CASE(oxygen) CASE(guards) CASE(empty) CASE(ammo) CASE(notices)
 CASE(shaft) CASE(bomb) CASE(mainroom) CASE(freeze) CASE(gauntlet) CASE(loading) CASE(identity) CASE(failures) CASE(reset) CASE(alias) CASE(transmission) CASE(owners)
 CASE(rollback_replaced) CASE(rollback_failed_write) CASE(rollback_replaced_during_restore)
 return 2;
}
