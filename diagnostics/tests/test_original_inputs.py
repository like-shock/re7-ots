"""Source-derived native producer fixtures; no SDK/Lua or live process needed.

Would catch pre/post FOV confusion, invalid reads treated as safe zeros,
low-word weapon publication loss, gunblock timeout off-by-one, character 3
float/numeric comparison confusion, grab-duration/order and load reset errors.
"""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
HARNESS = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "original_inputs.h"
#include "native_sites.h"
typedef struct { uint64_t at; unsigned n; unsigned char data[64]; } Region;
static Region regions[96]; static unsigned count; static uint64_t failed_address, changed_address;
static unsigned changed_reads, read_calls;
static Re7OriginalInputs in; static Re7OriginalStateConfig config;
static NativeFrame f; static float xmm[16];
static const uint64_t base=0x140000000ULL, player=0x200003000ULL;
static const uint64_t aim=0x300005020ULL, inventory=0x400006010ULL, head=0x500006000ULL;
float native_get_xmm_scalar(const void *p,unsigned reg){return ((const float*)p)[reg];}
static void put(uint64_t at,const void *data,unsigned n){
 for(unsigned i=0;i<count;i++)if(regions[i].at==at){regions[i].n=n;memcpy(regions[i].data,data,n);return;}
 for(unsigned i=0;i<count;i++)if(at>=regions[i].at&&at-regions[i].at<=regions[i].n&&n<=regions[i].n-(at-regions[i].at)){
  memcpy(regions[i].data+(at-regions[i].at),data,n);return;}
 assert(count<96&&n<=64); regions[count].at=at;regions[count].n=n;memcpy(regions[count++].data,data,n);
}
static void q(uint64_t at,uint64_t value){put(at,&value,8);}
static void d(uint64_t at,uint32_t value){put(at,&value,4);}
static void b(uint64_t at,uint8_t value){put(at,&value,1);}
static void fl(uint64_t at,float value){put(at,&value,4);}
static int rd(void *ctx,uint64_t at,void *out,size_t n){
 (void)ctx;read_calls++;if(at==failed_address)return 0;
 if(at==changed_address&&n==8&&changed_reads++){uint64_t other=0x260000000ULL;memcpy(out,&other,8);return 1;}
 for(unsigned i=0;i<count;i++)if(at>=regions[i].at&&at-regions[i].at<=regions[i].n&&n<=regions[i].n-(at-regions[i].at)){
  memcpy(out,regions[i].data+(at-regions[i].at),n);return 1;}
 return 0;
}
static void call(unsigned site){re7_original_inputs_capture(&in,site,&f,xmm,base,rd,0,&config);}
static void fixture(void){
 count=0;failed_address=changed_address=0;changed_reads=0;memset(&f,0,sizeof f);memset(xmm,0,sizeof xmm);
 re7_original_state_defaults(&config);re7_original_inputs_init(&in);
 q(base+0x8FB8018,0x200001000ULL);q(0x2000010D8ULL,0x200002000ULL);q(0x200002018ULL,player);
 Re7OriginalVec p={1,2,3};put(player+0x30,&p,12);
 q(base+0x8FBB610,0x300001000ULL);q(0x300001020ULL,0x300002000ULL);
 q(0x300002088ULL,0x300003000ULL);q(0x300003010ULL,0x300004000ULL);q(0x3000040A8ULL,aim-0x20);b(aim,0);
 q(0x3000010A0ULL,0x500001000ULL);q(0x5000010E0ULL,0x500002000ULL);q(0x500002078ULL,0x500003000ULL);
 q(0x500003010ULL,0x500004000ULL);q(0x500004018ULL,0x500005000ULL);q(0x5000050C0ULL,head);fl(head+0x53C,1);fl(head+0x4BC,2);
 q(base+0x8FAC380,0x400001000ULL);q(0x400001208ULL,0x400002000ULL);q(0x400002058ULL,0x400003000ULL);
 q(0x400003148ULL,0x400004000ULL);q(0x400004030ULL,0x400005000ULL);q(0x400005048ULL,inventory-0x10);b(inventory,0);
 q(base+0x8FB17F0,0);call(NS_CUTSCENE_AUTOLOOK);
 f.rax=0x600000000ULL;d(f.rax+0x54,5);call(NS_LOADED_AREA);fl(f.rax+0xB4,75);xmm[0]=15;call(NS_FOV_AUTOOLDCAM_WRITE);
 f.rbx=0x700000000ULL;fl(f.rbx+0x348,1);call(NS_TIMESCALE_MULTIPLIER);
}
static void check_defaults(void){
 re7_original_inputs_init(&in);assert(in.state.framerate==60&&in.state.flashlightflag==1);
 assert(in.state.characters==(1u<<RE7_ORIGINAL_CHAR_JOE));assert(!in.ready&&!in.state.player_available);
 assert(!in.state.model_marker_available&&!in.observed&&in.gbaimcheck==0);
}
static void check_chains(void){
 fixture();assert(in.player_base==player&&in.aim_address==aim&&in.inventory_address==inventory&&in.head_address==head);
 assert(in.state.player_available&&in.state.player.x==1&&in.state.player.y==2&&in.state.player.z==3);
 assert(in.ready);b(aim,2);b(inventory,1);call(NS_AFTER_ADAPTIVE_DIFF);assert(in.state.aim==2&&in.state.inventory==1);
 failed_address=aim;call(NS_AFTER_ADAPTIVE_DIFF);assert(!in.ready&&in.state.aim==2&&(in.failed&RE7_INPUT_AIM));
 failed_address=0;b(aim,0);call(NS_AFTER_ADAPTIVE_DIFF);assert(in.ready&&in.state.aim==0);
 failed_address=0x400003148ULL;call(NS_CUTSCENE_AUTOLOOK);assert(!in.ready&&(in.failed&RE7_INPUT_INVENTORY));
}
static void check_optional_model(void){
 fixture();config.autodetectmodel=1;call(NS_AFTER_ADAPTIVE_DIFF);assert(in.state.model_marker_available&&in.state.model_marker==1);
 in.state.characters=1u<<RE7_ORIGINAL_CHAR_MAIN_CLANCY;call(NS_AFTER_ADAPTIVE_DIFF);assert(in.state.model_marker==2);
 failed_address=head+0x4BC;call(NS_AFTER_ADAPTIVE_DIFF);assert(!in.state.model_marker_available&&in.ready);
}
static void check_fov_weapon(void){
 fixture();assert(in.state.autooldcamfov==75);f.rax=0x600000000ULL;failed_address=f.rax+0xB4;call(NS_FOV_AUTOOLDCAM_WRITE);
 assert(in.state.autooldcamfov==75&&!in.ready);failed_address=0;
 f.rbx=0x700000000ULL;d(f.rbx+0x3B0,500);call(NS_HELD_WEAPON_CMP);assert(in.heldweaponid==500&&in.state.holdinggun==1);
 d(f.rbx+0x3B0,0x100001F4);call(NS_HELD_WEAPON_CMP);assert(in.heldweaponid==500&&in.weapon_raw==0x100001F4&&!in.state.holdinggun);
}
static void check_character(void){
 fixture();f.rdi=0x800000000ULL;d(f.rdi+0x1E8,3);fl(player+0x38,50);call(NS_CHARACTER_CMP);
 assert(in.state.characters==(1u<<RE7_ORIGINAL_CHAR_PRESENT_MIA));fl(player+0x38,-5);call(NS_CHARACTER_CMP);
 assert(in.state.characters==(1u<<RE7_ORIGINAL_CHAR_MAIN_CLANCY));
 d(f.rdi+0x1E8,1);call(NS_CHARACTER_CMP);assert(in.state.characters==(1u<<RE7_ORIGINAL_CHAR_CHRIS));
 d(f.rdi+0x1E8,0);in.state.loadarea=3;call(NS_CHARACTER_CMP);assert(in.state.characters==(1u<<RE7_ORIGINAL_CHAR_JOE));
 in.state.loadarea=5;call(NS_CHARACTER_CMP);assert(in.state.characters==(1u<<RE7_ORIGINAL_CHAR_ETHAN));
 failed_address=f.rdi+0x1E8;call(NS_CHARACTER_CMP);assert(in.failed&RE7_INPUT_CHARACTER);
}
static void check_gun_timer(void){
 fixture();xmm[0]=.1f;call(NS_GUN_BLOCK_DIS_CALC);assert(in.state.aimblockedflag&&in.gbaimcheck==0);
 in.oldaim_address=in.aim_address;for(int k=0;k<3;k++)call(NS_TIMER_AOB);assert(in.state.aimblockedflag);
 call(NS_TIMER_AOB);assert(!in.state.aimblockedflag&&in.gbaimcheck==4);
 xmm[0]=-.1f;call(NS_NAH_GUN_BLOCK_DIS_CALC);assert(!in.state.aimblockedflag&&in.gbaimcheck==0);
 in.frametrack=1001;call(NS_TIMER_AOB);assert(in.frametrack==1);
 in.oldaim_address=0;in.state.igtimer=10;call(NS_TIMER_AOB);assert(in.state.loadscreenflag&&in.state.igtimer==0);
 f.rdi=0x800000000ULL;fl(f.rdi+0x50,2);call(NS_BETTER_TIMER);call(NS_TIMER_AOB);assert(!in.state.loadscreenflag);
 in.state.loadarea=35;fl(f.rbx+0x2D4,9);call(NS_TIMER_AOB);assert(in.state.igtimer==9);
}
static void check_event_flags(void){
 fixture();f.rax=0x600000000ULL;b(f.rax+0x53,0);call(NS_LADDER_CMP);assert(in.state.ladder==1&&in.ladder_address==f.rax+0x53);
 b(in.ladder_address,0);call(NS_AFTER_ADAPTIVE_DIFF);assert(!in.state.ladder);
 f.rbx=0x700000000ULL;b(f.rbx+0x144,2);call(NS_CROUCH_FLAG_READ);assert(in.state.crouchflag==2);
 fl(f.rbx+0xAC,1);call(NS_NO_GUN_ACCESS_FLOAT);assert(!in.state.nogunaccess);fl(f.rbx+0xAC,0);call(NS_NO_GUN_ACCESS_FLOAT);assert(in.state.nogunaccess);
 b(f.rbx+0x60,2);call(NS_TELESCOPE_FLAG);assert(in.state.telescopeflag);b(f.rbx+0x60,0);call(NS_TELESCOPE_FLAG);assert(!in.state.telescopeflag);
 f.rcx=0x800000000ULL;f.rbp=0xAB02;call(NS_FLASHLIGHT_WRITE);assert(in.state.flashlightflag==2&&in.flashlight_address==f.rcx+0x60);
}
static void check_timescale(void){
 fixture();xmm[0]=1.f/60;call(NS_TIMESCALE_FRAMERATE);assert(in.state.framerate==60);
 q(base+0x8FB17F0,0x10000);call(NS_TIMESCALE_MULTIPLIER);assert(in.state.inexitdelay==0);
 q(base+0x8FB17F0,1);call(NS_TIMESCALE_MULTIPLIER);assert(in.state.inexitdelay==17);
 q(base+0x8FB17F0,0);call(NS_TIMESCALE_MULTIPLIER);assert(in.state.inexitdelay==16);
 in.state.recentfpflag=1;call(NS_TIMESCALE_MULTIPLIER);assert(!in.state.recentfpflag);
 fl(f.rbx+0x348,0);call(NS_TIMESCALE_MULTIPLIER);xmm[0]=0;call(NS_TIMESCALE_FRAMERATE);assert(in.state.framerate==60);
}
static void check_grab(void){
 fixture();f.rdx=0x800000000ULL;b(f.rdx+0xA8,1);call(NS_GRAB_FLAG_WHEN_ACTIVE_CMP);
 assert(in.grab_address==f.rdx+0xA8);call(NS_TIMESCALE_MULTIPLIER);assert(in.state.moldgrabflag&&in.extendedgrabframes==14);
 b(in.grab_address,0);in.state.autooldcamfov=70;call(NS_TIMESCALE_MULTIPLIER);assert(in.extendedgrabframes==14&&in.state.moldgrabflag);
 in.state.autooldcamfov=75;for(int k=0;k<14;k++)call(NS_TIMESCALE_MULTIPLIER);assert(in.extendedgrabframes==0&&in.state.moldgrabflag);
 call(NS_TIMESCALE_MULTIPLIER);assert(!in.state.moldgrabflag);
 b(in.grab_address,1);in.state.loadarea=4;Re7OriginalVec p={-13,0,0};put(player+0x30,&p,12);call(NS_TIMESCALE_MULTIPLIER);assert(!in.state.moldgrabflag);
 in.state.loadscreenflag=1;call(NS_TIMESCALE_MULTIPLIER);assert(!in.grab_address&&!in.ladder_address&&!in.ready);
}
static void check_body_guard(void){
 fixture();Re7OriginalVec p={-3,-5,14};put(player+0x30,&p,12);call(NS_TIMESCALE_MULTIPLIER);assert(in.state.bodybagarea);
 in.state.autooldcamfov=70;p.x=30;put(player+0x30,&p,12);call(NS_TIMESCALE_MULTIPLIER);assert(in.state.bodybagarea);
 in.state.autooldcamfov=75;call(NS_TIMESCALE_MULTIPLIER);assert(!in.state.bodybagarea);
 p=(Re7OriginalVec){0,-7.6f,4};put(player+0x30,&p,12);call(NS_TIMESCALE_MULTIPLIER);assert(!in.state.bodybagarea);
 p=(Re7OriginalVec){40,-.6f,30};put(player+0x30,&p,12);call(NS_TIMESCALE_MULTIPLIER);in.heldweaponid=1802;f.r9=1;xmm[6]=.5f;call(NS_GUARD_FLOAT);assert(in.state.guardflag);
 f.r9=0;call(NS_GUARD_FLOAT);assert(!in.state.guardflag);
}
static void check_retry_failed_event(void){
 fixture();f.rbx=0x700000000ULL;d(f.rbx+0x3B0,500);failed_address=f.rbx+0x3B0;
 call(NS_HELD_WEAPON_CMP);assert(!in.ready&&(in.failed&RE7_INPUT_WEAPON));
 failed_address=0;call(NS_AFTER_ADAPTIVE_DIFF);assert(in.ready&&in.state.holdinggun&&!(in.failed&RE7_INPUT_WEAPON));
 b(f.rbx+0x60,2);failed_address=f.rbx+0x60;call(NS_TELESCOPE_FLAG);assert(!in.ready);
 failed_address=0;call(NS_AFTER_ADAPTIVE_DIFF);assert(in.ready&&in.state.telescopeflag);
}
static void check_bad_fp_and_input(void){
 fixture();uint32_t nan=0x7FC00000;memcpy(&xmm[6],&nan,4);call(NS_GUARD_FLOAT);assert(!in.ready&&(in.failed&RE7_INPUT_GUARD));
 fl(f.rbx+0x180,.5f);call(NS_AFTER_ADAPTIVE_DIFF);assert(in.ready&&!in.state.guardflag);
 uint64_t observed=in.observed;call(NATIVE_SITE_COUNT);assert(in.observed==observed);
 in.oldaim_address=in.aim_address;call(NS_TIMER_AOB);assert(in.state.frametrack==in.frametrack&&in.frame==1);
 call(NS_RUN_FLAG_ON);assert(in.state.runflag==1);f.rax=999;call(NS_RUN_FLAG_OFF);assert(in.state.runflag==0);
}
static void check_changed_chain(void){
 fixture();NativeFrame before=f;float xmm_before[16];memcpy(xmm_before,xmm,sizeof xmm);
 changed_address=base+0x8FB8018;call(NS_CUTSCENE_AUTOLOOK);
 assert(!in.ready&&!in.state.player_available&&!in.player_base&&(in.failed&RE7_INPUT_PLAYER));
 assert(!memcmp(&before,&f,sizeof f)&&!memcmp(xmm_before,xmm,sizeof xmm));
 changed_address=0;call(NS_CUTSCENE_AUTOLOOK);assert(in.ready&&in.state.player_available);
}
static void check_hot_path_reads(void){
 fixture();unsigned before=read_calls;call(NS_HEAD_Y_WRITE);assert(read_calls==before);
 b(aim,1);call(NS_CAM_WRITES);assert(read_calls==before+1&&in.state.aim==1);
 config.fpaimflag=1;b(aim,0);call(NS_HEAD_Y_WRITE);assert(read_calls==before+2&&in.state.aim==0);
}
int main(int argc,char **argv){assert(argc==2);switch(atoi(argv[1])){
 case 0:check_defaults();break;case 1:check_chains();break;case 2:check_optional_model();break;
 case 3:check_fov_weapon();break;case 4:check_character();break;case 5:check_gun_timer();break;
 case 6:check_event_flags();break;case 7:check_timescale();break;case 8:check_grab();break;case 9:check_body_guard();break;
 case 10:check_retry_failed_event();break;case 11:check_bad_fp_and_input();break;
 case 12:check_changed_chain();break;
 case 13:check_hot_path_reads();break;
 default:abort();}return 0;}
'''


class OriginalInputTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.tmp.cleanup)
        harness = Path(cls.tmp.name)/'inputs_harness.c'
        harness.write_text(HARNESS)
        cls.binary = Path(cls.tmp.name)/'inputs_harness'
        subprocess.run(['cc', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                        '-ffp-contract=off', '-fno-fast-math', '-I', str(ROOT/'src'),
                        str(harness), str(ROOT/'src/original_inputs.c'),
                        str(ROOT/'src/original_state.c'), str(ROOT/'src/original_scalar.c'),
                        str(ROOT/'src/original_collision.c'), '-lm', '-o', str(cls.binary)], check=True)

    def run_case(self, number):
        subprocess.run([str(self.binary), str(number)], check=True)


for _n, _name in enumerate(('defaults', 'chains_failure_recovery', 'optional_model',
                            'fov_old_value_weapon_word', 'character_conflicts',
                            'gun_timeout_timer_order', 'event_flags', 'timescale_order',
                            'grab_extension_reset', 'bodybag_guard',
                            'failed_event_recovers_on_policy_tick', 'invalid_fp_and_frame_publication',
                            'changed_chain_and_register_preservation', 'hot_path_reads_follow_source_gates')):
    setattr(OriginalInputTests, 'test_' + _name, lambda self, n=_n: self.run_case(n))

if __name__ == '__main__':
    unittest.main()
