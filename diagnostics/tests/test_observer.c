#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "observer.h"
/* Address-space fixture: real bytes, no dereferencing game-like addresses. */
typedef struct {u64 a; u8 b[32]; usize n;} Cell;
static Cell cells[48]; static int nc, calls, fail_call, mutate_call; static u64 mutate_addr;
static void put(u64 a,const void*p,usize n){cells[nc].a=a;cells[nc].n=n;memcpy(cells[nc++].b,p,n);}
static void ptr(u64 a,u64 v){put(a,&v,8);}
static int rd(void*ctx,u64 a,void*p,usize n){(void)ctx;calls++;if(calls==fail_call)return 0; if(calls==mutate_call){for(int i=0;i<nc;i++)if(cells[i].a==mutate_addr)cells[i].b[0]^=0x10;}for(int i=0;i<nc;i++)if(cells[i].a==a&&cells[i].n>=n){memcpy(p,cells[i].b,n);return 1;}return 0;}
static const u64 base=0x140000000;
static void fixture(void){nc=calls=fail_call=mutate_call=0;float xyz[]={1.25f,-2.5f,3.75f};u8 flag=1;u64 raw=0x12340000;
 ptr(base+0x8FB8018,0x100000);ptr(0x100000+0xD8,0x110000);ptr(0x110000+0x18,0x120000);put(0x120030,xyz,12);
 ptr(base+0x8FBB610,0x200000);ptr(0x200020,0x210000);ptr(0x210088,0x220000);ptr(0x220010,0x230000);ptr(0x2300A8,0x240000);put(0x240020,&flag,1);
 ptr(base+0x8FAC380,0x300000);ptr(0x300208,0x310000);ptr(0x310058,0x320000);ptr(0x320148,0x330000);ptr(0x330030,0x340000);ptr(0x340048,0x350000);put(0x350010,&flag,1);
 put(base+0x8FB17F0,&raw,8);
}
int main(void){ObsState state={0};ObsSample s;fixture();Cell before[48];memcpy(before,cells,sizeof cells);obs_sample(rd,0,base,1,&state,&s);
 assert(s.player.status==OBS_OK&&s.player.xyz[0]==1.25f&&s.player.xyz[1]==-2.5f&&s.player.xyz[2]==3.75f);assert(s.aim.status==OBS_OK&&s.aim.raw==1&&s.inventory.raw==1);assert(s.interact.raw==0x12340000);assert(s.player.generation==1);assert(!memcmp(before,cells,sizeof cells));assert(calls<=64);
 calls=0;obs_sample(rd,0,base,1,&state,&s);assert(s.player.generation==1);
 for(int hop=1;hop<=4;hop++){fixture();fail_call=hop;obs_sample(rd,0,base,1,&state,&s);assert(s.player.status==OBS_READ_FAILED&&s.player.fail_step==(u32)(hop-1));assert(s.player.xyz[0]==0);}
 fixture();memset(cells[1].b,0,8);obs_sample(rd,0,base,1,&state,&s);assert(s.player.status==OBS_NULL&&s.player.fail_step==1);
 fixture();u64 bad=~(u64)0;memcpy(cells[0].b,&bad,8);obs_sample(rd,0,base,1,&state,&s);assert(s.player.status==OBS_BAD_POINTER);
 fixture();mutate_call=4;mutate_addr=base+0x8FB8018;obs_sample(rd,0,base,1,&state,&s);assert(s.player.status==OBS_CHANGED&&s.player.xyz[0]==0);
 fixture();u32 nan=0x7fc00000;memcpy(cells[3].b,&nan,4);obs_sample(rd,0,base,1,&state,&s);assert(s.player.status==OBS_INVALID_VALUE);
 fixture();cells[9].b[0]=7;obs_sample(rd,0,base,1,&state,&s);assert(s.aim.status==OBS_INVALID_VALUE&&s.aim.raw==7);
 fixture();calls=0;obs_sample(rd,0,base,0,&state,&s);assert(calls==0&&s.player.status==OBS_UNSUPPORTED);
 fixture();obs_sample(rd,0,base,1,&state,&s);u32 gen=s.player.generation;fixture();fail_call=1;obs_sample(rd,0,base,1,&state,&s);fixture();obs_sample(rd,0,base,1,&state,&s);assert(s.player.generation==gen+1);
 fixture();u64 changed=0x130000;memcpy(cells[2].b,&changed,8);cells[3].a=0x130030;obs_sample(rd,0,base,1,&state,&s);assert(s.player.status==OBS_OK&&s.player.generation==gen+2);
 fixture();obs_sample(rd,0,~(u64)0-4,1,&state,&s);assert(s.player.status==OBS_BAD_POINTER);
 puts("observer: 12 scenarios PASS (safe reads, mutation, reload, semantics, immutable fixture)");return 0;}
