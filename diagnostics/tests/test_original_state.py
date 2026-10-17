"""Source-derived native state behavior: CT 2858-3018, 5795-6231, 7567-8647.

Would catch collapsed stop/internal-FP flags, proximity-generated FP, missing
weapon categories, focus on non-guns/cutscenes, changed strict boundaries,
loss of transition frame staging, or idle-state instability. Tests compile
and invoke the real C module; no game pointers or mock policy implementation.
"""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
HARNESS = r'''
#include "original_state.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define EQ(a,b) assert((a)==(b))
#define NEAR(a,b) assert(fabsf((a)-(b)) < .00001f)
static Re7OriginalStateConfig c;
static Re7OriginalStateInput in;
static Re7OriginalState s;
static Re7OriginalParams p;
static Re7OriginalFocusState f;
static Re7OriginalStateRates r;
static void init(void) {
 re7_original_state_defaults(&c); memset(&in,0,sizeof(in)); memset(&s,0,sizeof(s));
 memset(&f,0,sizeof(f)); re7_original_defaults(&p);
 in.player_available=1; in.loadarea=5; in.autooldcamfov=75; in.framerate=60;
 in.player=(Re7OriginalVec){100,100,100}; in.camera=in.player;
 re7_original_state_rates(&c,&p,60,&r);
}
static unsigned cam(void) {return re7_original_state_camera(&c,&in,&s,&p,&f,&r);}
static void policy(void) {re7_original_state_policy(&c,&in,&s);}
int main(int argc,char **argv) {
 assert(argc==2); int t=atoi(argv[1]); init();
 switch(t) {
 case 0: { /* 811-846, 993-1002: original defaults, not distance policy */
  EQ(c.adisablecheck,1); EQ(c.fptptransitionsflag,1); EQ(c.gbflag,0);
  EQ(c.inventorydisable,1); EQ(c.fpmeleeflag,1); EQ(c.truegbusesfocus,1);
  for(int k=0;k<1000;k++){policy();EQ(cam(),RE7_ORIGINAL_CAMERA_TP);}
  EQ(s.stopflag,0); EQ(s.internalfpflag,0); EQ(s.blockedgun,0);
  NEAR(p.aimbehinddis,1); NEAR(p.aimsidedis,1); NEAR(p.heightdisadjust,.9f);
  break; }
 case 1: /* enablestop is staged; alwaysstop is immediate */
  in.inventory=1; policy(); EQ(s.fakegbflag,1); EQ(s.blockedgun,1); EQ(s.stopflag,0);
  c.fptptransitionsflag=0; policy(); EQ(s.stopflag,1);
  memset(&s,0,sizeof(s)); in.inventory=0; in.telescopeflag=1; policy();
  EQ(s.stopflag,1); EQ(s.stoppedbloodhide,1); EQ(s.fakegbflag,0); break;
 case 2: /* code2 clears only these three; transition flags survive */
  s.stopflag=s.blockedgun=s.stoppedbloodhide=s.fakegbflag=s.truegbflag=s.internalfpflag=1;
  policy(); EQ(s.stopflag,0); EQ(s.blockedgun,0); EQ(s.stoppedbloodhide,0);
  EQ(s.fakegbflag,1); EQ(s.truegbflag,1); EQ(s.internalfpflag,1);
  c.adisablecheck=0; policy(); EQ(s.fakegbflag,0); EQ(s.truegbflag,0); break;
 case 3: { /* weapon IDs use unsigned exclusive upper bounds */
  const uint32_t ids[]={98,99,198,199,298,299,398,399,498,499,1798,1799,2298,2299,
    2998,2999,3398,3399,4298,4299,4398,4399,4698,4699,4898,4899,5298,5299,5698,
    5699,5798,5799,6098,6099,6198,6199,6298,6299,6398,6399,6698,6699,6798,6799,0xffffffffu};
  const unsigned kinds[]={0,2,2,2,2,2,2,2,2,1,1,0,0,0,0,0,0,2,2,2,2,1,1,2,2,1,1,0,0,2,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
  for(unsigned k=0;k<sizeof(ids)/sizeof(ids[0]);k++)EQ(re7_original_state_weapon(&c,ids[k]),kinds[k]);
  c.tp_melee[RE7_ORIGINAL_MELEE_KNIFE]=1; EQ(re7_original_state_weapon(&c,300),0);
  c.tp_melee[RE7_ORIGINAL_MELEE_CROWBAR]=2; EQ(re7_original_state_weapon(&c,400),2);
  break; }
 case 4: /* no automatic FP from ordinary collision or proximity */
  in.camera=in.player; in.aimblockedflag=1; policy(); EQ(s.blockedgun,0); EQ(s.stopflag,0);
  c.gbflag=1; policy(); EQ(s.blockedgun,1); EQ(s.truegbflag,1); EQ(s.fakegbflag,0);
  memset(&s,0,sizeof(s)); in.crouchflag=1; policy(); EQ(s.blockedgun,0);
  in.crouchflag=0; in.itempickuppop=1; policy(); EQ(s.blockedgun,0); break;
 case 5: /* 5849-5893 focus selection */
  in.aim=1; in.holdinggun=1; EQ(cam(),0); assert(p.aimbehinddis<1); EQ(f.unzoomflag,1);
  init(); in.aim=1; in.holdinggun=0; cam(); NEAR(p.aimbehinddis,1);
  init(); in.aim=1; in.holdinggun=1; in.autooldcamfov=73; cam(); NEAR(p.aimbehinddis,1);
  in.moldgrabflag=1; cam(); assert(p.aimbehinddis<1);
  init(); in.aim=1; in.holdinggun=1; in.inexitdelay=1; cam(); NEAR(p.aimbehinddis,1);
  c.grabzoomin=1; in.moldgrabflag=1; cam(); assert(p.aimbehinddis<1); break;
 case 6: /* explicit FP has priority except active transition */
  in.firstpersonflag=1; EQ(cam(),1); in.firstpersonflag=0;
  c.fpaimflag=1; in.aim=1; EQ(cam(),1); in.aim=2; EQ(cam(),0);
  c.thirdpersonflag=0; EQ(cam(),1); break;
 case 7: /* 6010: reaches threshold this call, TP remains for that Z */
  s.blockedgun=s.fakegbflag=1; p.aimbehinddis=.36f; r.gbaimtick=.02f;
  EQ(cam(),0); EQ(s.gbdelayset,1); EQ(s.readjustxyzfp,1); EQ(s.internalfpflag,0);
  EQ(s.gununblockdelay2,120); NEAR(p.aimsidedis,0);
  EQ(cam(),0); EQ(s.internalfpflag,1); EQ(s.readjustxyzfp,2);
  EQ(cam(),1); NEAR(p.aimbehinddis,r.clippedgbheadhidedis); EQ(s.gununblockdelay2,5); break;
 case 8: /* 6114: two different first unzoom frames before regular TP */
  s.fakegbflag=s.gbprocessflag=s.gbdelayset=1; s.readjustxyzfp=2;
  p.aimbehinddis=.35f; p.aimsidedis=0; p.heightdisadjust=0;
  EQ(cam(),1); EQ(s.readjustxyzfp,3); EQ(s.gbunzoomingprocessflag,1);
  EQ(cam(),0); EQ(s.readjustxyzfp,4); EQ(s.internalfpflag,1);
  EQ(cam(),0); EQ(s.internalfpflag,0);
  for(int k=0;k<200;k++){cam();} EQ(s.gbprocessflag,0); EQ(s.fakegbflag,0);
  NEAR(p.heightdisadjust,.9f); break;
 case 9: /* true gun-block focus skips internal FP but keeps original branch */
  s.blockedgun=s.truegbflag=1; p.aimbehinddis=.45f; p.aimsidedis=1.3f;
  cam(); EQ(s.gbdelayset,1); assert(p.aimsidedis>1.3f); EQ(s.internalfpflag,0);
  EQ(cam(),1); EQ(s.internalfpflag,0); /* 6094-6097 jumps oldsideaimheadcheck */
  s.blockedgun=0; in.aim=0; s.gununblockdelay2=1;
  EQ(cam(),0); EQ(s.gununblockdelay2,0); EQ(s.internalfpflag,0); break;
 case 10: /* persistent flags / unsigned pre-step thresholds */
  s.gbprocessflag=1; s.fakegbflag=1; s.gbdelayset=1; s.gununblockdelay2=8;
  in.itempickuppop=1; cam(); EQ(s.gununblockdelay2,8);
  in.itempickuppop=0; in.inventory=1; cam(); EQ(s.gbunzoomingprocessflag,1);
  init(); p.aimbehinddis=-1; cam(); NEAR(p.aimbehinddis,0);
  break;
 case 11: /* cutscene FOV zero ignored, 74 is inclusive non-stop */
  in.autooldcamfov=0; policy(); EQ(s.blockedgun,0);
  in.autooldcamfov=74; policy(); EQ(s.blockedgun,0);
  in.autooldcamfov=73; policy(); EQ(s.fakegbflag,1); EQ(s.blockedgun,1);
  memset(&s,0,sizeof(s)); in.ladder=1; policy(); EQ(s.blockedgun,0);
  in.ladder=0; in.moldgrabflag=1; policy(); EQ(s.blockedgun,0);
  c.adonlygrabs=1; in.nogunaccess=0; policy(); EQ(s.blockedgun,1); break;
 case 12: /* crawlspace exact location policy, existing FP changes entry */
  in.player=(Re7OriginalVec){10,-1,12}; policy(); EQ(s.blockedgun,1); EQ(s.stopflag,0);
  in.firstpersonflag=1; policy(); EQ(s.stopflag,1);
  memset(&s,0,sizeof(s)); in.player.x=7; policy(); EQ(s.blockedgun,1); EQ(s.stopflag,0);
  memset(&s,0,sizeof(s)); c.noclipautodisable=1; policy(); EQ(s.blockedgun,0); break;
 case 13: /* delayed puzzle strict > timer, original pointer low-word test */
  c.loccheckflag=0; /* bypass literal 8006 @f -> 8038, reach puzzlecheck */
  in.player=(Re7OriginalVec){-6,.35f,2}; in.interaction_pointer=1; in.igtimer=10;
  policy(); EQ(s.timertripflag,1); EQ(s.stopflag,0); NEAR(s.endtime,10.6f);
  in.igtimer=10.6f; policy(); EQ(s.stopflag,0);
  in.igtimer=10.7f; policy(); EQ(s.stopflag,1); EQ(s.stoppedbloodhide,1);
  in.interaction_pointer=0x10000; policy(); EQ(s.timertripflag,0); EQ(s.stopflag,0); break;
 case 14: /* rate math clips threshold, never initiates FP */
  NEAR(r.focus.aimtick,.55f/60/.25f); NEAR(r.focus.sideaimtick,.3f/60/.25f);
  NEAR(r.focus.heighttick,.6f/60/.25f); NEAR(r.gbaimtick,.65f/60/.4f);
  p.camclipbehinddis=.3f; re7_original_state_rates(&c,&p,60,&r);
  NEAR(r.clippedgbheadhidedis,.9f); NEAR(r.clippedgbheadhidediscrouch,.9f);
  EQ(s.internalfpflag,0); break;
 case 15: /* hard character/model stop is optional, no model prerequisite */
  in.model_marker_available=0; policy(); EQ(s.stopflag,0);
  in.model_marker_available=1; in.model_marker=.5f; policy(); EQ(s.stopflag,1); EQ(s.assumeheadless,1);
  in.model_marker=1; policy(); EQ(s.stopflag,0); EQ(s.assumeheadless,0);
  c.autodisable_character[RE7_ORIGINAL_CHAR_ETHAN]=1; in.characters=1u<<RE7_ORIGINAL_CHAR_ETHAN;
  policy(); EQ(s.stopflag,1); EQ(s.assumeheadless,1); break;
 case 16: /* original stops and disabled transitions cleanup */
  s.stopflag=1; s.gbprocessflag=1; s.fakegbflag=1;
  p.aimbehinddis=1.04f; p.aimsidedis=1.06f; EQ(cam(),0);
  EQ(s.gbprocessflag,0); EQ(s.fakegbflag,0); NEAR(p.aimbehinddis,1.04f); NEAR(p.aimsidedis,1);
  init(); c.fptptransitionsflag=0; s.blockedgun=s.fakegbflag=1;
  EQ(cam(),1); EQ(s.internalfpflag,1); break;
 case 17: /* actual melee -> gun -> empty changes policy cleanly */
  in.holdingmelee=(re7_original_state_weapon(&c,300)==2); policy(); EQ(s.fakegbflag,1);
  in.holdingmelee=0; in.holdinggun=(re7_original_state_weapon(&c,500)==1); policy(); EQ(s.blockedgun,0);
  in.holdinggun=(re7_original_state_weapon(&c,0)==1); EQ(in.holdinggun,0); break;
 case 18: /* 8006 @f -> 8038 bypasses TP inspection preference */
  in.interactableflag=1; policy(); EQ(s.stopflag,1); EQ(s.stoppedbloodhide,1);
  memset(&s,0,sizeof(s)); c.loccheckflag=0; policy(); EQ(s.stopflag,0); break;
 case 19: /* Daughters phone miss @f bypasses disabled location options */
  c.loccheckflag=0; c.noclipautodisable=1; in.loadarea=32;
  in.player=(Re7OriginalVec){20,-.7f,12}; policy(); EQ(s.blockedgun,1); break;
 case 20: /* bedroom @f reaches bad-transition tail even with FOV option off */
  c.loccheckflag=0; c.fovcheckflag=0; in.loadarea=29;
  in.player=(Re7OriginalVec){-9,4,0}; in.autooldcamfov=73;
  policy(); EQ(s.blockedgun,1); EQ(s.fakegbflag,1); break;
 case 21: /* 5879 @f -> 5890 skips grab focus when available aim byte is zero */
  c.grabzoomin=1; in.moldgrabflag=1; cam();
  NEAR(p.aimbehinddis,1); EQ(f.unzoomflag,0);
  in.inexitdelay=1; cam(); assert(p.aimbehinddis<1); EQ(f.unzoomflag,1); break;
 case 22: /* Final boss IGT window clears stop without clearing latched fake */
  in.loadarea=6; in.player=(Re7OriginalVec){-107,0,-127}; in.camera=in.player;
  in.autooldcamfov=73; in.igtimer=10; policy(); EQ(s.blockedgun,1);
  in.igtimer=22; policy(); EQ(s.blockedgun,0); EQ(s.fakegbflag,1); NEAR(s.finaligtimepassed,12);
  in.igtimer=41; policy(); EQ(s.blockedgun,1); break;
 case 23: { /* 2524-2549 frame equality check precedes all mutation */
  p.heightdisadjust=-1; p.backdisadjust=32; p.sidedisadjust=INFINITY;
  Re7OriginalParams before=p; Re7OriginalStateRates oldrates=r;
  EQ(re7_original_state_frame(&c,&in,&s,&p,&r),0); EQ(memcmp(&before,&p,sizeof(p)),0);
  in.frametrack=1; EQ(re7_original_state_frame(&c,&in,&s,&p,&r),1);
  NEAR(p.heightdisadjust,.9f); NEAR(p.backdisadjust,1.3f); NEAR(p.sidedisadjust,.3f);
  EQ(s.frametrackfovrecord,1); p.aimbehinddis=.5f; before=p;
  EQ(re7_original_state_frame(&c,&in,&s,&p,&r),0); EQ(memcmp(&before,&p,sizeof(p)),0);
  EQ(memcmp(&oldrates,&r,sizeof(r)),0); break; }
 case 24: { /* 2706-2712 immediate FP resets distances/delay, retains tick rates */
  in.frametrack=2; in.firstpersonflag=1; s.gununblockdelay2=123;
  p.aimbehinddis=.35f; p.aimsidedis=0; p.heightdisadjust=0;
  r.focus.aimtick=.123f; Re7OriginalStateRates before=r;
  EQ(re7_original_state_frame(&c,&in,&s,&p,&r),1);
  EQ(s.gununblockdelay2,0); NEAR(p.aimbehinddis,1); NEAR(p.aimsidedis,1);
  NEAR(p.heightdisadjust,.9f); EQ(memcmp(&before,&r,sizeof(r)),0);
  in.frametrack=3; s.gbprocessflag=1; p.aimbehinddis=.35f; s.gununblockdelay2=77;
  re7_original_state_frame(&c,&in,&s,&p,&r); NEAR(p.aimbehinddis,.35f);
  EQ(s.gununblockdelay2,77); assert(r.focus.aimtick!=.123f); break; }
 case 25: /* 2551-2605 unsigned pre-step run thresholds allow overshoot */
  c.runningfov=1; c.maxrunpulldis=1.5f; c.runbaserate=.6f; c.siderunbaserate=.6f;
  in.runflag=1; in.frametrack=1; p.backdisadjust=1.5f; p.sidedisadjust=.2f;
  re7_original_state_frame(&c,&in,&s,&p,&r); NEAR(p.backdisadjust,1.51f); NEAR(p.sidedisadjust,.19f);
  in.frametrack=2; re7_original_state_frame(&c,&in,&s,&p,&r);
  NEAR(p.backdisadjust,1.51f); NEAR(p.sidedisadjust,.19f);
  in.frametrack=3; in.runflag=0; p.backdisadjust=1.305f; p.sidedisadjust=.295f;
  re7_original_state_frame(&c,&in,&s,&p,&r); NEAR(p.backdisadjust,1.295f); NEAR(p.sidedisadjust,.305f);
  in.frametrack=0; re7_original_state_frame(&c,&in,&s,&p,&r); EQ(s.frametrackfovrecord,0); break;
 case 26: /* 8121 default-dword memory compare differs from r8w tests */
  c.loccheckflag=0; in.loadarea=29; in.player=(Re7OriginalVec){-11.5f,4,7};
  in.interaction_pointer=UINT64_C(0x100000000); s.bedclockframes=3;
  policy(); EQ(s.bedclockframes,0); EQ(s.stopflag,0);
  in.interaction_pointer=0x10000; s.bedclockframes=3;
  policy(); EQ(s.bedclockframes,3); EQ(s.stopflag,0); break;
 default: assert(0);
 }
 return 0;
}
'''

class OriginalStateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.tmp.cleanup)
        harness = Path(cls.tmp.name)/'state_harness.c'
        harness.write_text(HARNESS)
        cls.binary = Path(cls.tmp.name)/'state_harness'
        subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror',
                        '-ffp-contract=off','-fno-fast-math','-I',str(ROOT/'src'),
                        str(harness),str(ROOT/'src/original_state.c'),
                        str(ROOT/'src/original_scalar.c'),'-lm','-o',str(cls.binary)],check=True)

    def run_case(self, index):
        subprocess.run([str(self.binary),str(index)],check=True)

for _n, _name in enumerate(('idle_defaults','stop_vs_internal_fp','clear_semantics','weapon_boundaries',
    'no_proximity_fallback','focus_selection','explicit_fp','blocked_entry_staging',
    'unzoom_staging','true_block_focus','delay_and_negative_bits','cutscene_policy',
    'crawlspaces','puzzle_timer','source_rates','optional_model','transition_cleanup','weapon_switches',
    'literal_inspection_jump','literal_daughters_jump','literal_bedroom_jump','literal_grab_jump','final_boss_timer',
    'frame_cadence_and_clamps','frame_explicit_fp_reset','frame_run_pull','interaction_operand_width')):
    setattr(OriginalStateTests, 'test_'+_name, lambda self,n=_n:self.run_case(n))


# Optional private-source differential validation. No CT text is distributed.
# It interprets only the audited policy block and fails on unknown instructions;
# actual @f labels are resolved by position, not by nearby comments or names.
import ctypes as C
import hashlib
import os
import random
import re
import struct


def _bits(v):
    return struct.unpack('<I',struct.pack('<f',v))[0]


def _float(v):
    return struct.unpack('<f',struct.pack('<I',v & 0xffffffff))[0]


class _Vec(C.Structure):
    _fields_=[(n,C.c_float) for n in ('x','y','z')]


class _Config(C.Structure):
    _flags=('adisablecheck thirdpersonflag fpaimflag fptptransitionsflag zoomaimflag '
        'guncheckflag grabzoomin truegbusesfocus gbaimdelay fovcheckflag inventorydisable '
        'loccheckflag autodetectmodel fpmeleeflag fpflashlight manualflashflag critterexamine '
        'tpinteractables adonlygrabs floorgrabad gbflag gbcrouchdisable noclipautodisable ffscriptallowfp').split()
    _fields_=[(n,C.c_uint8) for n in _flags]+[
        ('tp_melee',C.c_uint8*13),('autodisable_character',C.c_uint8*8),
        ('autodisable_location',C.c_uint8*14)]+[(n,C.c_float) for n in (
        'minaimbehinddis maxaimsidedis aimforwardzoomrate gbheadhidedis '
        'gbheadhidediscrouch gbfowardaimrate gununblockdelay').split()]+[('runningfov',C.c_uint8)]+[
        (n,C.c_float) for n in ('maxrunpulldis','runbaserate','sideminrunpulldis','siderunbaserate')]


class _Input(C.Structure):
    _flags=('player_available flycamflag firstpersonflag recentfpflag aim inventory ladder '
        'crouchflag holdinggun holdingmelee telescopeflag guardflag flashlightflag interactableflag '
        'lockboxflag bodybagarea loadscreenflag moldgrabflag nogunaccess itempickuppop aimblockedflag '
        'ffscripton ffscriptfpexception model_marker_available').split()
    _fields_=[(n,C.c_uint8) for n in _flags]+[('flash',C.c_uint16)]+[
        (n,C.c_uint32) for n in ('loadarea','characters','inexitdelay')]+[
        ('interaction_pointer',C.c_uint64),('player',_Vec),('camera',_Vec)]+[
        (n,C.c_float) for n in ('autooldcamfov','igtimer','framerate','model_marker')]+[('runflag',C.c_uint8),('frametrack',C.c_uint32)]


class _State(C.Structure):
    _flags=('stopflag blockedgun internalfpflag truegbflag fakegbflag gbprocessflag gbdelayset '
        'gbunzoomingprocessflag gunblockunzoombackdoneflag stoppedbloodhide assumeheadless '
        'badtransitionflag spiderpuzfpvhsexc timertripflag finaligtimereset').split()
    _fields_=[(n,C.c_uint8) for n in _flags]+[(n,C.c_uint32) for n in (
        'readjustxyzfp','gununblockdelay2','bedclockframes')]+[(n,C.c_float) for n in (
        'starttime','endtime','finaligtimer','finaligtimepassed')]+[('frametrackfovrecord',C.c_uint32)]


class _PolicyOracle:
    def __init__(self,path,camera=False):
        self.camera=camera
        lo,hi=(5816,6295) if camera else (7567,8647)
        self.code=[]; self.labels={}; self.anonymous=[]
        for raw in path.read_text().splitlines():
            m=re.match(r'0*(\d+):\s*(.*)',raw)
            if not m or not lo<=int(m[1])<=hi:continue
            line=m[2].split('//')[0].strip()
            if not line or line.startswith('{$'):continue
            if line.endswith(':'):
                if line=='@@:':self.anonymous.append(len(self.code))
                else:self.labels[line[:-1]]=len(self.code)
            else:self.code.append((int(m[1]),line))

    def run(self,c,i,s,params=None,focus=None,rates=None):
        data={}; regs={'rax':0,'r8':0}; stack=[]; fp=[]; cmp=0
        def put_struct(obj):
            for name,ty in obj._fields_:
                value=getattr(obj,name)
                if ty==C.c_float:data[name]=_bits(value)
                elif isinstance(value,int):data[name]=value
        put_struct(c);put_struct(i);put_struct(s)
        if self.camera:
            put_struct(params);put_struct(focus);put_struct(rates);put_struct(rates.focus)
            if data['aimbehinddis']>=0x42000000:data['aimbehinddis']=0
        data.update(_playerbase=i.player_available,headyadd=0x100000,
            invflagadd=0x200000,ladflagadd=0x300000,aimflagadd=0x400000,delaytime=_bits(.6))
        for n,axis in enumerate('xyz'):
            data['cplayer'+axis]=_bits(getattr(i.player,axis))
            data['cam'+axis+'attempt']=_bits(getattr(i.camera,axis))
        chars=('isethan ismainclancy ispresentmia ispastmia isdlcclancy ischris isjoe iszoe').split()
        for n,name in enumerate(chars):data[name]=int(bool(i.characters&(1<<n)))
        adchars=('ethan maingame_clancy present_mia past_mia dlc_clancy chris joe zoe').split()
        for n,name in enumerate(adchars):data['autodisable_'+name]=c.autodisable_character[n]
        adloc=('ethan_main_game clancy_derelict_house_tape clancy_happy_birthday_tape '
            'mia_recreation_room_tape mia_wrecked_ship mia_past_ship_tape clancy_nightmare_dlc '
            'clancy_bedroom_dlc clancy_21_dlc zoe_daughters_dlc chris_not_a_hero_dlc '
            'joe_end_of_zoe_dlc mia_jacks_55th_birthday_dlc ethan_ethan_must_die_dlc').split()
        for n,name in enumerate(adloc):data['ad_'+name]=c.autodisable_location[n]
        aliases={'eax':('rax',32),'ax':('rax',16),'al':('rax',8),'r8d':('r8',32),'r8w':('r8',16)}
        def clean(v):return re.sub(r'^(byte|word|dword|qword) ptr ','',v).strip()
        def read(v):
            v=clean(v)
            if v in aliases:
                name,bits=aliases[v];return regs[name]&((1<<bits)-1)
            if v in regs:return regs[v]
            if v.startswith('['):
                key=v[1:-1]
                if key=='re7.exe+8FB17F0':return i.interaction_pointer
                if key=='r8':
                    addr=regs['r8']
                    if addr in (0x10053c,0x1004bc):
                        return _bits(i.model_marker if i.model_marker_available else 1)
                    if addr==0x200000:return i.inventory
                    if addr==0x300000:return i.ladder
                    if addr==0x400000:return i.aim
                    raise AssertionError(('unmapped read',addr))
                return data[key]
            if v.startswith('(float)'):return _bits(float(v[7:]))
            if v.startswith('#'):return int(v[1:])
            return int(v,16)
        def write(dst,value):
            dst=clean(dst)
            if dst.startswith('['):data[dst[1:-1]]=value&0xffffffff;return
            if dst in aliases:
                name,bits=aliases[dst]; mask=(1<<bits)-1
                regs[name]=(0 if bits==32 else regs[name]&~mask)|(value&mask)
            else:regs[dst]=value
        pc=0 if self.camera else self.labels['newmem2']; steps=0; result=0
        while steps<2000:
            steps+=1; line_no,line=self.code[pc]; pc+=1
            op,_,tail=line.partition(' ');args=[a.strip() for a in tail.split(',')]
            if op=='mov':
                value=read(args[1])
                if args[0].startswith('[') and args[1] in ('ax','al'):
                    mask=0xffff if args[1]=='ax' else 0xff
                    value=(read(args[0])&~mask)|(value&mask)
                write(args[0],value)
            elif op=='lea':
                m=re.fullmatch(r'\[r8\+([0-9A-F]+)\]',args[1]);assert m
                write(args[0],regs['r8']+int(m[1],16))
            elif op=='cmp':
                left,right=read(args[0]),read(args[1])
                if args[0]=='[re7.exe+8FB17F0]':left &= 0xffffffff
                cmp=(left>right)-(left<right)
            elif op=='push':stack.append(read(args[0]))
            elif op=='pop':write(args[0],stack.pop())
            elif op=='inc':write(args[0],read(args[0])+1)
            elif op=='dec':write(args[0],read(args[0])-1)
            elif op=='fld':fp.append(_float(read(args[0])))
            elif op=='fadd':fp[-1]+=_float(read(args[0]))
            elif op=='fsub':fp[-1]-=_float(read(args[0]))
            elif op=='fmul':fp[-1]*=_float(read(args[0]))
            elif op=='fdiv':fp[-1]/=_float(read(args[0]))
            elif op=='fistp':write(args[0],round(fp.pop()))
            elif op=='fchs':fp[-1]=-fp[-1]
            elif op=='fstp':write(args[0],_bits(fp.pop()))
            elif op in ('xorps','maxss'):pass
            elif op in ('jmp','je','jne','ja','jb','jae','jbe'):
                jump={'jmp':True,'je':cmp==0,'jne':cmp!=0,'ja':cmp>0,'jb':cmp<0,'jae':cmp>=0,'jbe':cmp<=0}[op]
                if jump:
                    if tail=='return2':break
                    if tail in ('thirdaimoldz','oldsideaimheadcheck'):
                        result=int(tail=='oldsideaimheadcheck');break
                    if tail=='@f':pc=next(a for a in self.anonymous if a>=pc)
                    else:pc=self.labels[tail]
            else:raise AssertionError(('unknown instruction',line_no,line))
        else:raise AssertionError('policy did not return')
        assert not stack and not fp
        return ({name:data[name] for obj in (s,params,focus) for name,_ in obj._fields_},result) if self.camera else {name:data[name] for name,_ in s._fields_}


class OriginalStatePrivateSourceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        path=Path(os.environ.get('RE7_ORIGINAL_NUMBERED_SOURCE',
            ROOT.parent/'original/RE7_OTS_RT_source_numbered.txt'))
        if not path.exists():raise unittest.SkipTest('private source not supplied; set RE7_ORIGINAL_NUMBERED_SOURCE')
        if hashlib.sha256(path.read_bytes()).hexdigest()!='20e58c365dd4f76df9e78c41dc41e3799dd52085de02a5372271416c1a3cedac':
            raise AssertionError('Numbered source hash does not match private RT 8.02 reference')
        cls.oracle=_PolicyOracle(path)
        cls.camera_oracle=_PolicyOracle(path,camera=True)
        cls.tmp=tempfile.TemporaryDirectory();cls.addClassCleanup(cls.tmp.cleanup)
        so=Path(cls.tmp.name)/'state.so'
        subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-shared','-fPIC',
            '-ffp-contract=off','-fno-fast-math',str(ROOT/'src/original_state.c'),
            str(ROOT/'src/original_scalar.c'),'-lm','-o',str(so)],check=True)
        cls.lib=C.CDLL(str(so))
        cls.lib.re7_original_state_defaults.argtypes=[C.POINTER(_Config)]
        cls.lib.re7_original_state_policy.argtypes=[C.POINTER(_Config),C.POINTER(_Input),C.POINTER(_State)]

    def test_policy_matches_original_instructions_and_anonymous_jumps(self):
        rng=random.Random(8012)
        locations=[(5,(100,100,100)),(5,(10,-1,12)),(5,(7,-1,12)),(10,(0,-4,80)),
            (6,(70,12,15)),(6,(74,12,21)),(37,(-25,-.5,-335)),(37,(-30,4,-336.7)),
            (35,(90,-75,-180)),(32,(20,-.7,12)),(32,(20,0,4)),(5,(-6,.35,2)),
            (10,(-20,-2.95,92)),(5,(11,8,10)),(5,(28,-1.82,19)),(11,(54,-.6,22)),
            (5,(-11.5,4,7)),(29,(-11,4,3)),(29,(-9,4,0)),(29,(-9,4,3.5)),
            (6,(-107,0,-127)),(7,(18,-7,16)),(4,(-48.5,0,122.5)),(6,(45,0,65)),
            (5,(-6,-.5,-14)),(5,(31,1.49,0)),(5,(21,4,3)),(5,(14,4,11)),
            (5,(10.5,0,2)),(6,(80,25.4,26)),(29,(-8,4,3.5)),(5,(31.5,0,2.5))]
        for index in range(1500):
            c=_Config();self.lib.re7_original_state_defaults(C.byref(c));i=_Input();s=_State()
            area,xyz=locations[index%len(locations)];i.player=_Vec(*xyz);i.camera=_Vec(*xyz)
            i.player_available=1;i.loadarea=area;i.autooldcamfov=rng.choice((0,73,74,75));i.framerate=60
            i.igtimer=rng.choice((0,.05,10,10.6,10.7,63,70));i.interaction_pointer=rng.choice((0,1,0x10000))
            s.starttime=10;s.endtime=10.6
            for flag in ('firstpersonflag','interactableflag','inventory','ladder','moldgrabflag','nogunaccess',
                         'bodybagarea','itempickuppop','aimblockedflag','crouchflag','ffscripton','ffscriptfpexception'):
                setattr(i,flag,rng.randrange(2) if rng.random()<.2 else 0)
            for flag in ('fovcheckflag','loccheckflag','tpinteractables','adonlygrabs','floorgrabad','gbflag','noclipautodisable'):
                setattr(c,flag,rng.randrange(2))
            for flag in _State._flags:setattr(s,flag,rng.randrange(2) if rng.random()<.2 else 0)
            expected=self.oracle.run(c,i,s)
            self.lib.re7_original_state_policy(C.byref(c),C.byref(i),C.byref(s))
            actual={name:_bits(getattr(s,name)) if ty==C.c_float else getattr(s,name) for name,ty in s._fields_}
            self.assertEqual(actual,expected,f'case {index} area={area} xyz={xyz}')

class _Params(C.Structure):
    _fields_=[(n,C.c_float) for n in ('backdisadjust behinddis aimbehinddis camclipbehinddis pitchmul '
        'sidedisadjust sidedis aimsidedis camclipsidedis heightdis heightdisadjust '
        'camclipheightdis yadd headadjust sideaimadjust firstyadd tppitchadj').split()]


class _Focus(C.Structure):
    _fields_=[('unzoomflag',C.c_uint8),('unzoomsidedoneflag',C.c_uint8)]


class _FocusRates(C.Structure):
    _fields_=[(n,C.c_float) for n in ('aimtick','sideaimtick','heighttick','minaimbehinddis','maxaimsidedis')]


class _Rates(C.Structure):
    _fields_=[('focus',_FocusRates)]+[(n,C.c_float) for n in (
        'gbaimtick','gbsideaimtick','gbheighttick','clippedgbheadhidedis','clippedgbheadhidediscrouch')]


def _test_camera_matches_private_source(self):
    self.lib.re7_original_state_camera.argtypes=[C.POINTER(_Config),C.POINTER(_Input),C.POINTER(_State),
        C.POINTER(_Params),C.POINTER(_Focus),C.POINTER(_Rates)]
    self.lib.re7_original_state_camera.restype=C.c_uint
    self.lib.re7_original_defaults.argtypes=[C.POINTER(_Params)]
    self.lib.re7_original_state_rates.argtypes=[C.POINTER(_Config),C.POINTER(_Params),C.c_float,C.POINTER(_Rates)]
    rng=random.Random(6231)
    for index in range(3000):
        c=_Config();i=_Input();s=_State();p=_Params();f=_Focus();r=_Rates()
        self.lib.re7_original_state_defaults(C.byref(c));self.lib.re7_original_defaults(C.byref(p))
        i.framerate=rng.choice((30,60,120));i.autooldcamfov=rng.choice((0,73,74,75));i.player=_Vec(100,100,100)
        for flag in ('firstpersonflag','recentfpflag','aim','holdinggun','inventory','crouchflag','itempickuppop','moldgrabflag'):
            setattr(i,flag,rng.choice((0,0,1,2)))
        for flag in ('stopflag','blockedgun','gbprocessflag','gbdelayset','internalfpflag','fakegbflag','truegbflag',
            'badtransitionflag','gbunzoomingprocessflag','gunblockunzoombackdoneflag'):
            setattr(s,flag,rng.choice((0,0,1,2)))
        for flag in ('thirdpersonflag','fpaimflag','fptptransitionsflag','truegbusesfocus','gbaimdelay','guncheckflag','grabzoomin','zoomaimflag'):
            setattr(c,flag,rng.choice((0,1,1,2)))
        s.readjustxyzfp=rng.randrange(5);s.gununblockdelay2=rng.choice((0,1,5,120,0x1ffff))
        i.inexitdelay=rng.choice((0,0,5));f.unzoomflag=rng.randrange(2);f.unzoomsidedoneflag=rng.randrange(2)
        p.aimbehinddis=rng.choice((-.1,0,.35,.45,.99,1,1.05,32))
        p.aimsidedis=rng.choice((-.1,0,.3,.99,1,1.05,1.3,1.4))
        p.heightdisadjust=rng.choice((0,.3,.899,.9));p.camclipbehinddis=rng.choice((.3,.7,1))
        self.lib.re7_original_state_rates(C.byref(c),C.byref(p),i.framerate,C.byref(r))
        expected,result=self.camera_oracle.run(c,i,s,p,f,r)
        actual_result=self.lib.re7_original_state_camera(C.byref(c),C.byref(i),C.byref(s),C.byref(p),C.byref(f),C.byref(r))
        actual={name:_bits(getattr(obj,name)) if ty==C.c_float else getattr(obj,name)
            for obj in (s,p,f) for name,ty in obj._fields_}
        self.assertEqual((actual,actual_result),(expected,result),f'camera case {index}; config='+str({n:getattr(c,n) for n in c._flags})+' input='+str({n:getattr(i,n) for n in i._flags})+' differences='+str({n:(actual[n],expected[n]) for n in expected if actual[n]!=expected[n]}))

OriginalStatePrivateSourceTests.test_camera_matches_original_instructions=_test_camera_matches_private_source

if __name__ == '__main__':
    unittest.main()
