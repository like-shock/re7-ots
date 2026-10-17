#include "original_state.h"
#include <float.h>
#include <math.h>
#include <string.h>
#include <limits.h>
#if LDBL_MANT_DIG != 64 || FLT_MANT_DIG != 24 || FLT_RADIX != 2
#error "Original state core requires x87 extended long double and binary32"
#endif
#ifdef __FAST_MATH__
#error "Original state core must not use fast math"
#endif
/* Preserve x87 stores and unsigned integer comparisons of float storage. */
static float f32(long double v) { volatile float out=(float)v; return out; }
static uint32_t u(float f) { uint32_t v; memcpy(&v,&f,4); return v; }
static int range(float f,float lo,float hi) { return u(f)>=u(lo)&&u(f)<=u(hi); }
static int interval(float f,float lo,float hi) { return u(f)>=u(lo)&&u(f)<u(hi); }
static int character(const Re7OriginalStateInput *in,unsigned n) {return (in->characters&(1u<<n))!=0;}
static uint32_t fistp32(long double v) {
    long double n=nearbyintl(v);
    return !isfinite(n)||n<INT32_MIN||n>INT32_MAX ? 0x80000000u : (uint32_t)(int32_t)n;
}

void re7_original_state_defaults(Re7OriginalStateConfig *c) {
    /* CT defaults 776-1017; adisablecheck=1 is enabled Auto-Disable script. */
    memset(c,0,sizeof(*c));
    c->adisablecheck=c->thirdpersonflag=c->fptptransitionsflag=1;
    c->zoomaimflag=c->guncheckflag=c->truegbusesfocus=c->gbaimdelay=1;
    c->fovcheckflag=c->inventorydisable=c->loccheckflag=c->autodetectmodel=1;
    c->fpmeleeflag=c->tpinteractables=c->floorgrabad=c->gbcrouchdisable=1;
    c->tp_melee[RE7_ORIGINAL_MELEE_CHAINSAW]=1;
    c->tp_melee[RE7_ORIGINAL_MELEE_AMG78A]=1;
    c->tp_melee[RE7_ORIGINAL_MELEE_JOE_FISTS]=1;
    c->tp_melee[RE7_ORIGINAL_MELEE_SPIRIT_BLADE]=1;
    c->tp_melee[RE7_ORIGINAL_MELEE_AMG_DUAL]=1;
    c->minaimbehinddis=.45f; c->maxaimsidedis=1.3f; c->aimforwardzoomrate=.25f;
    c->gbheadhidedis=.35f; c->gbheadhidediscrouch=.5f;
    c->gbfowardaimrate=.4f; c->gununblockdelay=2;
    c->maxrunpulldis=1.2f; c->runbaserate=.4f;
    c->sideminrunpulldis=.2f; c->siderunbaserate=.4f;
}

unsigned re7_original_state_weapon(const Re7OriginalStateConfig *c,uint32_t id) {
    /* HELD_WEAPON_CMP 2858-3018. TP melee is not a holdinggun. */
    static const struct { uint32_t upper; int melee; unsigned kind; } groups[]={
        {99,-1,0},{199,0,2},{299,1,2},{399,2,2},{499,3,2},{1799,-1,1},
        {2299,-1,0},{2999,4,2},{3399,-1,0},{4299,5,2},{4399,6,2},
        {4699,-1,1},{4899,7,2},{5299,-1,1},{5699,-1,0},{5799,8,2},
        {6099,-1,0},{6199,9,2},{6299,10,2},{6399,11,2},{6699,-1,0},{6799,12,2}
    };
    for (unsigned n=0;n<sizeof(groups)/sizeof(groups[0]);n++) if(id<groups[n].upper)
        return groups[n].melee>=0 && c->tp_melee[groups[n].melee]==1 ? 0 : groups[n].kind;
    return RE7_ORIGINAL_WEAPON_OTHER;
}

void re7_original_state_rates(const Re7OriginalStateConfig *c,const Re7OriginalParams *p,
                              float fps,Re7OriginalStateRates *r) {
    /* TRUE_FOV_READ 2608-2703. Explicit stores retain source operation order. */
    float rem=f32(fabsl(1.L-c->minaimbehinddis));
    r->focus.minaimbehinddis=c->minaimbehinddis;
    r->focus.maxaimsidedis=c->maxaimsidedis;
    r->focus.aimtick=f32(fabsl(1.L-c->minaimbehinddis)/fps/c->aimforwardzoomrate);
    float ratio=f32(fabsl(1.L-c->maxaimsidedis)/rem);
    r->focus.sideaimtick=f32((long double)r->focus.aimtick*ratio);
    ratio=f32((long double).6f/rem);
    r->focus.heighttick=f32((long double)r->focus.aimtick*ratio);
    r->clippedgbheadhidedis=f32((long double)c->gbheadhidedis/p->camclipbehinddis);
    r->clippedgbheadhidediscrouch=f32((long double)c->gbheadhidediscrouch/p->camclipbehinddis);
    if(u(r->clippedgbheadhidedis)>u(.9f))r->clippedgbheadhidedis=.9f;
    if(u(r->clippedgbheadhidediscrouch)>u(.9f))r->clippedgbheadhidediscrouch=.9f;
    float time=f32((long double)c->gbfowardaimrate*p->camclipbehinddis);
    r->gbaimtick=f32(fabsl(1.L-r->clippedgbheadhidedis)/fps/time);
    rem=f32(fabsl(1.L-r->clippedgbheadhidedis)); ratio=f32(1.L/rem);
    r->gbsideaimtick=f32((long double)r->gbaimtick*ratio);
    ratio=f32((long double).9f/rem);
    r->gbheighttick=f32((long double)r->gbaimtick*ratio);
}

void re7_original_state_policy(const Re7OriginalStateConfig *c,const Re7OriginalStateInput *in,
                               Re7OriginalState *s) {
    float x=in->player.x,y=in->player.y,z=in->player.z;
    uint32_t area=in->loadarea;
    int interact=(uint16_t)in->interaction_pointer!=0;
    if(c->adisablecheck!=1) { /* PLAYER_COORDS_WRITE 1110-1116 */
        s->stopflag=s->blockedgun=s->stoppedbloodhide=s->truegbflag=s->fakegbflag=0;
        return;
    }
    /* AFTER_ADAPTIVE_DIFF 7567-7803. */
    if(in->flycamflag==1)goto clear;
    if(!in->player_available)goto fovcheck;
    if(in->telescopeflag==1)goto bloodstop;
    if(in->guardflag==1)goto enablestop;
    s->assumeheadless=0;
    if(c->autodetectmodel!=0&&in->model_marker_available&&u(in->model_marker)!=u(1.f)) {
        s->assumeheadless=1; goto alwaysstop;
    }
    for(unsigned n=0;n<RE7_ORIGINAL_CHAR_COUNT;n++)
        if(c->autodisable_character[n]!=0&&character(in,n)){s->assumeheadless=1;goto alwaysstop;}
    const uint8_t *a=c->autodisable_location;
    if((a[0]&&character(in,0)&&area!=34)||(a[1]&&area==7)||(a[2]&&area==11)||
       (a[3]&&area==10)||(a[4]&&character(in,2)&&area==6)||(a[5]&&area==12)||
       (a[6]&&area==31)||(a[7]&&area==29)||(a[8]&&area==30)||
       (a[9]&&character(in,7))||(a[10]&&character(in,5))||(a[11]&&character(in,6))||
       (a[12]&&(area==22||area==23||area==24))||(a[13]&&area==34))goto alwaysstop;
    if(c->fpmeleeflag!=0&&in->holdingmelee==1)goto enablestop;
    if(c->fpflashlight!=0&&(in->flashlightflag==1||(c->manualflashflag!=0&&in->flash==1)))goto enablestop;
    if(c->inventorydisable!=0&&in->inventory==1)goto enablestop;
    if(c->critterexamine==1&&area==37&&in->interactableflag==1&&in->inventory==1)goto alwaysstop;
    if(in->bodybagarea==1)goto clear;
    /* 7805-7877 final boss timings, including timer reset compensation. */
    if(in->loadscreenflag==1){s->finaligtimer=0;s->finaligtimereset=0;}
    if(area==6&&!(range(in->camera.x,-107.f,-108.2f)&&u(in->camera.y)>=0x80000000u&&
                  range(in->camera.z,-127.4f,-129.f))&&range(x,-105.f,-110.f)&&range(z,-125.f,-130.f)) {
        if(u(in->igtimer)<=u(.1f)&&s->finaligtimereset!=1)
            s->finaligtimer=u(s->finaligtimepassed)>0x80000000u?s->finaligtimepassed:-s->finaligtimepassed;
        s->finaligtimereset=1;
        if(u(in->igtimer)>=u(.1f))s->finaligtimereset=0;
        if(u(s->finaligtimer)==0)s->finaligtimer=in->igtimer;
        s->finaligtimepassed=f32((long double)in->igtimer-s->finaligtimer);
        if(u(s->finaligtimepassed)>=u(11.5f)&&
           (u(s->finaligtimepassed)<u(30.f)||
            (u(s->finaligtimepassed)>=u(33.5f)&&u(s->finaligtimepassed)<u(37.3f))||
            u(s->finaligtimepassed)>=u(46.8f)))goto clear;
    }
    /* 7880-7963 exact scripted pre-emptive locations. */
    if(c->fovcheckflag==1) {
        if(area==7&&range(x,16,20)&&range(y,-6,-8)&&range(z,14.7f,18))goto alwaysstop;
        if(area==6&&u(in->autooldcamfov)!=u(75.f)&&range(x,43,50)&&range(z,62,73))goto alwaysstop;
        if(area==4&&range(x,-48,-49)&&range(z,122,123))goto enablestop;
        if(area==29&&range(x,-25,-28)&&range(z,5,6))goto alwaysstop;
    }
    if(area==32&&range(x,19,21)&&(u(y)<u(1.f)||range(y,-0.f,-1.f))) {
        if(range(z,3.7f,4.35f)&&range(in->igtimer,62,69))goto enablestop;
        /* @f at 7955/7957/7959/7961 targets 7975, bypassing options. */
        goto mainhouse_y;
    }
    /* 7965-8038: retain the literal anonymous-label targets. Several source
     * @f branches bypass the tpinteractables gate; interpreting their comments
     * as a regular list of independent location tests changes real behavior. */
    if(c->loccheckflag==0||c->noclipautodisable==1)goto interactable_gate;
    if(area!=5&&area!=32)goto mainhouse_fail;
mainhouse_y:
    if(interval(y,-.5f,-1.8f)&&range(x,6.85f,32)&&range(z,10.5f,17.2f)) {
        if(u(x)<u(8.f)||in->firstpersonflag==0)goto enablestop;
        goto alwaysstop;
    }
mainhouse_fail:
    if((area==5||area==10)&&interval(z,76.5f,88)&&
       (u(x)<u(3.6f)||range(x,-0.f,-3))&&range(y,-3.5f,-4.7f)) {
        if(in->firstpersonflag==0)goto enablestop;
        goto alwaysstop;
    }
    if(area==6) {
        if(u(x)<u(72.65f))goto forced_interactables;
        if(!(u(x)<u(75.15f)&&range(z,20.65f,22.65f))&&
           interval(y,11,14.35f)&&range(x,64,75)&&range(z,13,30)) {
            if(in->firstpersonflag==0)goto enablestop;
            goto alwaysstop;
        }
    }
    if(area!=37||u(x)<u(-22.f))goto forced_interactables;
    if(u(x)<u(-36.f)&&range(y,-.3f,-.8f)&&range(z,-333,-339)) {
        if(in->firstpersonflag==0)goto enablestop;
        goto alwaysstop;
    }
    if(u(x)<u(-28.5f))goto forced_interactables;
    if(u(x)<u(-33.f)&&range(y,3,5)&&range(z,-336.5f,-337))goto enablestop;
    /* 8019 cannot match 35 after reaching here through the preceding 37
     * checks; keep that literal branch rather than making it newly reachable. */
    if(area!=35||u(x)<u(79.f))goto forced_interactables;
    if(u(x)<u(101.f)&&range(y,-74.5f,-75.5f)&&range(z,-176,-184))goto enablestop;
interactable_gate:
    if(in->lockboxflag!=3&&c->tpinteractables==1&&in->firstpersonflag!=1)goto puzzles;
forced_interactables:
    if(in->interactableflag!=0)goto bloodstop;
    /* 8042-8087, 8515-8587: a matched box goes directly to doubleitemcheck,
     * even if the pointer is inactive. 8074/8076 @f instead targets 8127. */
    if((interval(x,-4.5f,-7)&&range(y,-.2f,-.8f)&&range(z,-12.2f,-15.2f))||
       (interval(x,29.85f,32.2f)&&range(y,1.48f,1.5f)&&
           (u(z)<u(.58f)||(u(z)>=0xb727c5adu&&u(z)<=u(-.72f))))||
       (interval(x,20.7f,22.6f)&&range(y,3.9f,4.1f)&&range(z,2.8f,3.9f))||
       (interval(x,13.5f,15.1f)&&range(y,3.9f,4.1f)&&range(z,10,12.2f))||
       (interval(x,10.1f,10.8f)&&u(y)<=0x3b03126fu&&range(z,1.6f,4))||
       (interval(x,79.7f,81)&&range(y,25.3f,25.5f)&&range(z,25.6f,27.9f)))goto doubleitem;
    if(area!=29||u(x)<u(-7.4f))goto resetclock;
    if(u(x)<u(-10.1f)&&range(z,3,4))goto doubleitem;
    goto puzzles;
doubleitem:
    if(interact)goto bloodstop;

puzzles: /* 8090-8154, 8410-8513 */
    s->spiderpuzfpvhsexc=0;
    if(in->interactableflag==1)goto fovcheck;
    if(u(y)>u(.27f)&&u(y)<=u(.405f)&&range(x,-5.47f,-9.17f)&&range(z,.9f,4.9f))goto delayedpuzzle;
    if(u(y)>u(-2.94f)&&u(y)<=u(-2.96f)&&range(x,-17.85f,-21.85f)&&range(z,90.95f,94.9f)) {
        s->spiderpuzfpvhsexc=1; goto delayedpuzzle;
    }
    if(u(y)>u(7.5f)&&u(y)<=u(8.5f)&&range(x,9.67f,12.36f)&&range(z,8.94f,12.91f))goto nodelaypuzzle;
    if(u(y)>u(-1.81f)&&u(y)<=u(-1.845f)&&range(x,26.29f,29.9f)&&range(z,18.05f,20.85f))goto delayedpuzzle;
    if(interval(y,-.596f,-.621f)&&(
       (range(x,53.5f,54.36f)&&range(z,21.45f,22.9f))||
       (range(x,52.04f,53.48f)&&range(z,35.85f,36.75f))||
       (range(x,42.85f,43.65f)&&range(z,25.7f,27.2f))))goto nodelaypuzzle;
    if(area==29&&(uint32_t)in->interaction_pointer!=0) {
        if(s->bedclockframes<=1){s->bedclockframes++;goto bedroom;}
    } else {
resetclock:
        s->bedclockframes=0;
    }
    if(interval(y,3.99f,4.01f)&&range(x,-10.89f,-12.51f)&&range(z,6.54f,8.24f))goto nodelaypuzzle;
bedroom:
    if(area!=29)goto fovcheck;
    /* @f at 8138/8143 targets 8216, inside the bad-transition checks. */
    if(u(x)<u(-10.4f))goto bad_after_garage;
    if(u(x)<u(-11.8f)&&range(z,2.6f,3.4f))goto nodelaypuzzle;
    if(u(x)<u(-8.2f))goto bad_after_garage;
    if(u(x)<u(-12.2f)&&(u(z)<u(1.3f)||range(z,-0.f,-.9f)))goto delayedpuzzle;
    goto fovcheck;
nodelaypuzzle:
    if(interact)goto bloodstop;
    goto fovcheck;
delayedpuzzle: /* 8293-8321 */
    if(!interact){s->timertripflag=0;goto fovcheck;}
    if(s->timertripflag!=1) {
        s->starttime=in->igtimer; s->endtime=f32((long double)s->starttime+.6f);
        s->timertripflag=1; goto fovcheck;
    }
    if(u(in->igtimer)>u(s->endtime))goto bloodstop;

fovcheck: /* 8156-8268; floor-grab comparison is a scripted-event exception. */
    if(c->adonlygrabs!=0) {
        if(!(c->floorgrabad==1&&in->nogunaccess!=0&&
             u(f32((long double)in->player.y+10000.8f))<=u(f32((long double)in->camera.y+10000.f)))&&
           in->moldgrabflag==1)goto grab_fov;
    }
    if(c->fovcheckflag==0||in->moldgrabflag==1||area==30)goto aimblock;
grab_fov:
    if(in->ladder==1)goto aimblock;
    if(in->moldgrabflag==1||u(in->autooldcamfov)>u(74.f))goto bad_done;
    if(area==5&&range(x,31.4f,31.7f)&&range(z,2.4f,2.7f)) {
        s->badtransitionflag=1;goto bad_done;
    }
bad_after_garage:
    s->badtransitionflag=(interval(y,-.94f,-1.13f)&&range(x,.75f,1.5f)&&range(z,80,82))||
        (area==35&&range(x,100,130)&&range(z,-70,-100))||
        (area==37&&range(x,-255,-264)&&range(y,5,5.5f)&&range(z,80,88));
bad_done:
    if(c->ffscriptallowfp!=1&&in->ffscriptfpexception!=1&&in->ffscripton==1&&in->moldgrabflag!=1)goto aimblock;
    if(u(in->autooldcamfov)!=0&&u(in->autooldcamfov)<u(74.f))goto enablestop;
aimblock: /* 8270-8291 */
    if(c->noclipautodisable!=1&&in->itempickuppop!=1&&c->gbflag!=0&&
       c->fptptransitionsflag!=0&&s->gbprocessflag!=1&&
       !(c->gbcrouchdisable!=0&&in->crouchflag==1)&&in->aimblockedflag==1) {
        s->blockedgun=1; s->truegbflag=1; return;
    }
clear: /* code2 8641-8647 does not clear the transition's latched flags. */
    s->stoppedbloodhide=s->stopflag=s->blockedgun=0; return;
enablestop: /* 8603-8623 */
    s->fakegbflag=1;
    if(c->fptptransitionsflag==1){s->blockedgun=1;return;}
    goto alwaysstop;
bloodstop:
    if(c->thirdpersonflag!=0)s->stoppedbloodhide=1;
alwaysstop:
    s->stopflag=1;
}

unsigned re7_original_state_camera(const Re7OriginalStateConfig *c,const Re7OriginalStateInput *in,
    Re7OriginalState *s,Re7OriginalParams *p,Re7OriginalFocusState *focus,const Re7OriginalStateRates *r) {
    /* 5795-5893: retain hook ordering and its distinction between published FP,
     * internal FP, and the branch actually selected for this one Z write. */
    if(u(p->aimbehinddis)>=0x42000000u)p->aimbehinddis=0;
    if(s->gbprocessflag!=1&&s->readjustxyzfp<3&&in->firstpersonflag==1)goto fp;
    if(c->fpaimflag==1&&in->aim==1)goto fp;
    if(c->thirdpersonflag==0)goto fp;
    if(s->stopflag==1)goto finish;
    if(s->gbprocessflag==1||s->blockedgun==1)goto blocked;
    s->readjustxyzfp=0;
    if(in->inexitdelay==0&&(in->moldgrabflag==1||
        (in->loadarea==6&&range(in->player.x,-105,-110)&&range(in->player.z,-125,-130))||
        u(in->autooldcamfov)>=u(74.f))) {
        /* 5879 je @f targets 5890: an available zero aim byte skips grabs. */
        if(in->aim==0)goto normal_unfocus;
        if(c->zoomaimflag==1) {
            re7_original_focus_step(p,focus,&r->focus,RE7_ORIGINAL_FOCUS_IN,c->guncheckflag,in->holdinggun);
            goto tp;
        }
    }
    if(c->grabzoomin==1&&in->moldgrabflag==1&&in->recentfpflag!=1) {
        re7_original_focus_step(p,focus,&r->focus,RE7_ORIGINAL_FOCUS_IN_NO_GUN_CHECK,c->guncheckflag,in->holdinggun);
        goto tp;
    }
normal_unfocus:
    if(focus->unzoomflag!=0)
        re7_original_focus_step(p,focus,&r->focus,RE7_ORIGINAL_FOCUS_OUT,c->guncheckflag,in->holdinggun);
    goto tp;
blocked: /* 5895-6030 */
    s->gbprocessflag=1;
    if(c->fptptransitionsflag!=1)goto delay;
    if(u(p->aimbehinddis)>=0x80000000u)p->aimbehinddis=0;
    if(u(p->aimsidedis)>=0x80000000u)p->aimsidedis=0;
    if(s->gbdelayset==1)goto aim_delay;
    if(c->truegbusesfocus==1&&s->truegbflag==1) {
        p->heightdisadjust=f32((long double)p->heightdisadjust-r->focus.heighttick);
        if(u(p->heightdisadjust)<u(.3f))p->heightdisadjust=.3f;
        if(u(p->aimbehinddis)>=u(r->focus.minaimbehinddis))p->aimbehinddis=f32((long double)p->aimbehinddis-r->focus.aimtick);
        if(u(p->aimsidedis)<=u(r->focus.maxaimsidedis))p->aimsidedis=f32((long double)p->aimsidedis+r->focus.sideaimtick);
        if(u(p->aimbehinddis)>=u(r->focus.minaimbehinddis)||u(p->aimsidedis)<=u(r->focus.maxaimsidedis))goto tp;
    } else {
        float threshold=in->crouchflag==1?r->clippedgbheadhidediscrouch:r->clippedgbheadhidedis;
        if(s->badtransitionflag==1){p->aimbehinddis=threshold;p->heightdisadjust=0;}
        p->heightdisadjust=f32((long double)p->heightdisadjust-r->gbheighttick);
        if(u(p->heightdisadjust)>=0x80000000u)p->heightdisadjust=0;
        p->aimbehinddis=f32((long double)p->aimbehinddis-r->gbaimtick);
        if(u(p->aimbehinddis)>=0x80000000u)p->aimbehinddis=0;
        p->aimsidedis=f32((long double)p->aimsidedis-r->gbsideaimtick);
        if(u(p->aimsidedis)>=0x80000000u)p->aimsidedis=0;
        if(u(p->aimbehinddis)>u(threshold))goto tp;
    }
    s->readjustxyzfp=1; s->gbdelayset=1;
    /* CT writes ax only into a dd here; retain the high word. */
    s->gununblockdelay2=(s->gununblockdelay2&0xffff0000u)|
        (fistp32((long double)in->framerate*c->gununblockdelay)&0xffffu);
    if(!(s->truegbflag==1&&c->truegbusesfocus==1))p->aimsidedis=0;
    goto tp;
aim_delay: /* 6032-6060 */
    if(c->gbaimdelay!=0&&s->gununblockdelay2!=0) {
        if(in->inventory==1)s->gununblockdelay2=0;
        if(in->aim!=0)s->gununblockdelay2=5;
    }
delay: /* 6062-6112 */
    if(s->gbunzoomingprocessflag==1)goto unzoom;
    if(s->fakegbflag==1) {
        s->internalfpflag=1;
        if(s->readjustxyzfp==1){s->readjustxyzfp=2;goto tp;}
        p->aimbehinddis=in->crouchflag==1?r->clippedgbheadhidediscrouch:r->clippedgbheadhidedis;
    }
    if(!(s->truegbflag==1&&c->truegbusesfocus==1))s->internalfpflag=1;
    if(s->blockedgun!=0){s->gununblockdelay2=5;goto fp;}
    if(s->gununblockdelay2==0)goto unzoom;
    if(in->itempickuppop!=1)s->gununblockdelay2--;
    if(s->truegbflag==1&&c->truegbusesfocus==1)goto tp;
    goto fp;
unzoom: /* 6114-6212 */
    s->gbunzoomingprocessflag=1;
    if(c->fptptransitionsflag==0)goto finish;
    if(s->stopflag!=0){s->internalfpflag=1;goto fp;}
    s->internalfpflag=0;
    if(s->fakegbflag!=1&&c->truegbusesfocus==1&&s->truegbflag==1) {
        p->heightdisadjust=f32((long double)p->heightdisadjust+r->focus.heighttick);
        if(u(p->heightdisadjust)>=u(.899f))p->heightdisadjust=.9f;
        if(u(p->aimbehinddis)<u(.99f))p->aimbehinddis=f32((long double)p->aimbehinddis+r->focus.aimtick);
        else s->gunblockunzoombackdoneflag=1;
        if(u(p->aimsidedis)>u(.99f)){p->aimsidedis=f32((long double)p->aimsidedis-r->focus.sideaimtick);goto tp;}
        if(s->gunblockunzoombackdoneflag==0)goto tp;
        goto finish;
    }
    p->heightdisadjust=f32((long double)p->heightdisadjust+r->gbheighttick);
    if(u(p->heightdisadjust)>=u(.899f))p->heightdisadjust=.9f;
    if(u(p->aimbehinddis)<u(.99f))p->aimbehinddis=f32((long double)p->aimbehinddis+r->gbaimtick);
    else s->gunblockunzoombackdoneflag=1;
    if(u(p->aimsidedis)<u(.99f)) {
        float tick=r->gbsideaimtick;
        if(in->crouchflag==1) {
            float ratio=f32((long double)r->clippedgbheadhidediscrouch/r->clippedgbheadhidedis);
            tick=f32((long double)r->gbsideaimtick*ratio);
        }
        p->aimsidedis=f32((long double)p->aimsidedis+tick);
    }
    if(s->readjustxyzfp==2){s->readjustxyzfp=3;s->internalfpflag=1;goto fp;}
    if(s->readjustxyzfp==3){s->readjustxyzfp=4;s->internalfpflag=1;goto tp;}
    if(s->gunblockunzoombackdoneflag==0)goto tp;
finish: /* 6214-6231 */
    if(u(p->aimbehinddis)>=u(1.05f))p->aimbehinddis=1;
    if(u(p->aimsidedis)>=u(1.05f))p->aimsidedis=1;
    p->heightdisadjust=.9f;
    s->truegbflag=s->fakegbflag=s->internalfpflag=s->gbdelayset=s->gbprocessflag=0;
    s->gunblockunzoombackdoneflag=s->gbunzoomingprocessflag=0;
tp:
    return RE7_ORIGINAL_CAMERA_TP;
fp:
    return RE7_ORIGINAL_CAMERA_FP;
}

int re7_original_state_frame(const Re7OriginalStateConfig *c,const Re7OriginalStateInput *in,
    Re7OriginalState *s,Re7OriginalParams *p,Re7OriginalStateRates *r) {
    /* TRUE_FOV_READ 2524-2605, 2706-2712; gate precedes even repairs. */
    if(in->frametrack==s->frametrackfovrecord)return 0;
    s->frametrackfovrecord=in->frametrack;
    if(u(p->heightdisadjust)>=0x42000000u)p->heightdisadjust=.9f;
    if(u(p->backdisadjust)>=0x42000000u)p->backdisadjust=1.3f;
    if(u(p->sidedisadjust)>=0x42000000u)p->sidedisadjust=.3f;
    if(s->gbprocessflag!=1&&in->firstpersonflag==1) {
        s->gununblockdelay2=0;
        p->backdisadjust=1.3f;p->sidedisadjust=.3f;
        p->aimbehinddis=p->aimsidedis=1;p->heightdisadjust=.9f;
        return 1;
    }
    if(c->runningfov==0) {
        p->backdisadjust=1.3f;p->sidedisadjust=.3f;
    } else {
        float tick=f32((long double)c->runbaserate/in->framerate);
        if(in->runflag!=0) {
            if(u(p->backdisadjust)<=u(c->maxrunpulldis))
                p->backdisadjust=f32((long double)p->backdisadjust+tick);
        } else if(u(p->backdisadjust)>u(1.3f))
            p->backdisadjust=f32((long double)p->backdisadjust-tick);
        tick=f32((long double)c->siderunbaserate/in->framerate);
        if(in->runflag!=0) {
            if(u(p->sidedisadjust)>=u(c->sideminrunpulldis)&&u(p->sidedisadjust)<=0x80000000u)
                p->sidedisadjust=f32((long double)p->sidedisadjust-tick);
        } else if(u(p->sidedisadjust)>0x80000000u||u(p->sidedisadjust)<u(.3f))
            p->sidedisadjust=f32((long double)p->sidedisadjust+tick);
    }
    re7_original_state_rates(c,p,in->framerate,r);
    return 1;
}
