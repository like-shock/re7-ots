/* RT 8.02 CT9274-9627,10229-10244. The HUD is enabled by default. Private
 * injection scratch registers/flags are not part of the replaced MOV spans.
 * A zero return always leaves the original frame and extended state intact. */
#include "native_runtime.h"
#include <string.h>
typedef struct {uint64_t locations[6],links[6],leaf;unsigned count;} Chain;
typedef struct {uint64_t address;uint8_t before,after;} Write;
static uint32_t bits(float f){uint32_t b;memcpy(&b,&f,4);return b;}
static int address(uint64_t base,uint64_t offset,size_t n,uint64_t*out){
 const uint64_t max=UINT64_C(0x7fffffffffff);
 if(base<0x10000||base>max||offset>max-base||n-1>max-base-offset)return 0;
 *out=base+offset;return 1;
}
static int read_at(NativeRuntime*r,uint64_t base,uint64_t offset,void*out,size_t n){
 uint64_t a;
 if(!address(base,offset,n,&a)||!r->read||!r->read(r->context,a,out,n)){
  r->stats.reads_failed++;return 0;
 }
 return 1;
}
static int write_at(NativeRuntime*r,uint64_t a,const uint8_t*value){
 if(!r->write||!r->write(r->context,a,value,1)){r->stats.writes_failed++;return 0;}return 1;
}
void native_hud_reset(NativeRuntime*r){r->hud=(NativeHudState){0};}
static int check_chain(NativeRuntime*r,const Chain*c){
 uint64_t value;
 for(unsigned j=c->count;j>0;j--)
  if(!read_at(r,c->locations[j-1],0,&value,8)||value!=c->links[j-1])return 0;
 return 1;
}
static int chain(NativeRuntime*r,Chain*c,const uint32_t*offsets,unsigned count,uint32_t leaf){
 uint64_t current=r->base;
 c->count=count;
 for(unsigned j=0;j<count;j++){
  if(!address(current,offsets[j],8,&c->locations[j])||
     !read_at(r,c->locations[j],0,&c->links[j],8))return 0;
  current=c->links[j];
 }
 return address(current,leaf,1,&c->leaf)&&check_chain(r,c);
}
static int display_live(NativeRuntime*r){
 uint32_t value;
 return r->hud.display&&read_at(r,r->hud.display,0x18,&value,4)&&value==r->hud.oxygen;
}
/* Source CMP/JB/JA use unsigned float encodings, including negative positions. */
static int between(float v,float low,float high){uint32_t b=bits(v);return b>=bits(low)&&b<=bits(high);}
static int region(const NativeRuntime*r,float x0,float x1,float z0,float z1){
 return between(r->inputs.state.player.x,x0,x1)&&between(r->inputs.state.player.z,z0,z1);
}
/* Lifecycle runs for every captured site, including producer calls during load.
 * Loading may seed story state but cannot authorize writes. A later oxygen
 * producer is required; repeated valid load observations retain those seeds. */
static int lifecycle(NativeRuntime*r,unsigned site){
 const uint64_t facts=RE7_INPUT_PLAYER|RE7_INPUT_AREA;
 if(!r->camera_enabled||(r->inputs.valid&facts)!=facts||
    !r->inputs.state.player_available||r->inputs.state.loadarea!=35){native_hud_reset(r);return 0;}
 if(r->hud.area!=35){native_hud_reset(r);r->hud.area=35;}
 if(r->inputs.state.loadscreenflag==1){
  if(!r->hud.loading){native_hud_reset(r);r->hud.area=35;r->hud.loading=1;}
  if(site==NS_NAH_HUD_WRITE){
   r->hud.bombtalk=r->hud.freezetalk=0;
   if(region(r,41,53,-91,-120))r->hud.freezetalk=1;
   if(region(r,78,100,-175,-184))r->hud.bombtalk=3;
  }
  return 0;
 }
 if(!r->inputs.ready){native_hud_reset(r);return 0;}
 r->hud.loading=0;return 1;
}
/* Return 0=show, 1=normal hide gate. CE's @f includes named labels: CT9558
 * goes to skipalltalkchecks, preserving the transmission1/FP checks. */
static int conversation(const NativeRuntime*r,NativeHudState*h){
 if(h->nahtrans==2)goto skip;
 if(region(r,-10,-20,-75,-90)){
  if(bits(r->inputs.state.igtimer)<bits(12.f)){h->nahtrans=1;return 0;}
  goto talking;
 }
 if(region(r,100,110,-220,-233)){
  if(h->bombtalk==3)goto skip;
  if(h->bombtalk==2){if(h->nahtalk==1)goto talking;h->bombtalk=3;goto skip;}
  if(h->bombtalk==1){if(!h->nahtalk)goto skip;h->bombtalk=2;goto talking;}
  if(h->nahtalk==1){h->bombtalk=1;h->nahtrans=2;}
  goto skip;
 }
 if(region(r,62,72,-179,-191)){
  h->bombtalk=0;
  if(h->stopmainroomtalk==2)goto skip;
  if(between(r->inputs.state.player.z,-179,-181)&&h->stopmainroomtalk!=1){h->stopmainroomtalk=2;goto skip;}
  h->stopmainroomtalk=1;goto talking;
 }
 if(!h->nahtalk)h->stopmainroomtalk=0;
 if(h->stopmainroomtalk==1&&h->nahtalk)goto talking;
 if(h->stopmainroomtalk==2&&h->nahtalk)goto skip;
 if(region(r,41,53,-91,-120)){
  if(h->freezetalk!=1&&h->nahtalk==1){h->freezetalk++;h->nahtrans=2;goto skip;}
  goto talking;
 }
 if(region(r,78,100,-175,-184)){
  if(h->bombtalk<3&&h->nahtalk==1){h->bombtalk++;h->nahtrans=2;goto skip;}
  goto talking;
 }
 goto skip;
talking:
 if(h->nahtalk==1){h->nahtrans=1;return 0;}
skip:
 if(!h->nahtalk)h->nahtrans=0;
 return h->nahtrans!=1;
}
static int add_write(NativeRuntime*r,Write*w,unsigned*n,uint64_t a,uint8_t value){
 w[*n].address=a;w[*n].after=value;
 if(!read_at(r,a,0,&w[*n].before,1))return 0;
 (*n)++;return 1;
}
static int overlaps(uint64_t a,uint64_t b,size_t n){return a>=b&&a-b<n;}
static int safe_destinations(const Write*w,unsigned n,const Chain*c,uint64_t stack,uint64_t display){
 const unsigned offsets[]={0x18,0x120,0x174,0x198};
 const unsigned sizes[]={4,2,2,2};
 for(unsigned j=0;j<n;j++){
  if(overlaps(w[j].address,stack,8))return 0;
  for(unsigned k=0;k<j;k++)if(w[j].address==w[k].address)return 0;
  for(unsigned k=0;k<4;k++)if(overlaps(w[j].address,display+offsets[k],sizes[k]))return 0;
  if(w[j].address==c[2].leaf)return 0; /* talk is sampled before stores */
  for(unsigned k=0;k<3;k++)for(unsigned l=0;l<c[k].count;l++)
   if(overlaps(w[j].address,c[k].locations[l],8))return 0;
 }
 return 1;
}
static int identities(NativeRuntime*r,const Chain*c){
 return check_chain(r,c)&&check_chain(r,c+1)&&check_chain(r,c+2)&&display_live(r);
}
static void rollback(NativeRuntime*r,const Chain*c,const Write*w,unsigned attempted){
 for(unsigned j=attempted;j>0;j--){
  /* Restoration is another game write. A retired/unreadable owner or changed
   * display sample ends rollback; saved bytes never authorize stale writes. */
  if(!identities(r,c))return;
  write_at(r,w[j-1].address,&w[j-1].before);
 }
}
static int hud_write(NativeRuntime*r,NativeFrame*f){
 static const uint32_t ammo[]={0x8fb5778,0x28,0x2b0,0x10,0x148,0x10};
 static const uint32_t half[]={0x8fb0ac8,0x58,0x10};
 static const uint32_t talk[]={0x8ff0c18,0x40,0x348,0x50};
 Chain c[3];Write w[4];unsigned n=0;uint64_t stack,hud,r12;uint8_t ammo_value;
 uint16_t contamination,notification,ramrod;NativeHudState next=r->hud;
 if((r->inputs.valid&(RE7_INPUT_WEAPON|RE7_INPUT_TIMER))!=(RE7_INPUT_WEAPON|RE7_INPUT_TIMER)||
    !display_live(r)||!chain(r,c,ammo,6,0x6c)||!chain(r,c+1,half,3,0x6c)||
    !chain(r,c+2,talk,4,0x328)||!read_at(r,c[0].leaf,0,&ammo_value,1)||
    !read_at(r,c[2].leaf,0,&next.nahtalk,1)||
    !read_at(r,next.display,0x120,&contamination,2)||
    !read_at(r,next.display,0x174,&notification,2)||!read_at(r,next.display,0x198,&ramrod,2)||
    !address(native_frame_game_rsp(f),0x38,8,&stack)||!read_at(r,stack,0,&r12,8)||
    !address(f->rdi,0x20,1,&hud))goto fail;
 /* Half/contamination stores precede the source ammo read. Overlap would
  * change that input, so this staged transaction must replay instead. */
 if(c[0].leaf==c[1].leaf||c[0].leaf==hud||c[0].leaf==next.display+0x128)goto fail;
 for(unsigned j=0;j<3;j++){
  size_t size=c[j].count*sizeof(uint64_t);
  if(next.have_owners&&memcmp(next.owner_links[j],c[j].links,size))goto fail;
  memcpy(next.owner_links[j],c[j].links,size);
 }
 next.have_owners=1;
 if(r->inputs.heldweaponid<=99){
  ammo_value=0;if(!add_write(r,w,&n,c[0].leaf,0))goto fail;
 }
 uint8_t half_value=1,hud_value=(uint8_t)f->r12;
 if(!(uint8_t)notification&&!(uint8_t)ramrod){
  int path=conversation(r,&next);
  if(path&&(r->state.stoppedbloodhide==1||r->view.firstpersonflag!=1)&&next.oxygen==0x42c80000u){
   if(r->state.stoppedbloodhide==1||ammo_value!=1){next.nahhudhidden=1;hud_value=0;}
   else if(next.nahhudhidden==1){next.nahhudhidden=0;hud_value=0;}
   else half_value=0;
  }
 }
 if(!add_write(r,w,&n,c[1].leaf,half_value))goto fail;
 if(r->view.firstpersonflag!=1){
  uint64_t a;if(!address(next.display,0x128,1,&a)||!add_write(r,w,&n,a,(uint8_t)contamination!=0))goto fail;
 }
 if(!add_write(r,w,&n,hud,hud_value)||!safe_destinations(w,n,c,stack,next.display))goto fail;
 /* Validate immediately before each store, including each restoration. Try
  * the failing store too: the write API may have copied a prefix. A lost
  * identity prevents rollback. R12/latches publish only after full success. */
 for(unsigned j=0;j<n;j++){
  if(!identities(r,c)){
   rollback(r,c,w,j);
   goto fail;
  }
  if(!write_at(r,w[j].address,&w[j].after)){
   rollback(r,c,w,j+1);
   goto fail;
  }
 }
 f->r12=r12;r->hud=next;return 1;
fail:
 native_hud_reset(r);return 0;
}
int native_hud_dispatch(NativeRuntime*r,unsigned site,NativeFrame*f,void*fp){
 if(!lifecycle(r,site))return 0;
 switch(site){
 case NS_NAH_OXYGEN_REF:{
  uint32_t value;float oxygen;
  if(!read_at(r,f->rbx,0x18,&value,4)){native_hud_reset(r);return 0;}
  if(r->hud.display&&r->hud.display!=f->rbx){native_hud_reset(r);r->hud.area=35;}
  r->hud.display=f->rbx;r->hud.oxygen=value;memcpy(&oxygen,&value,4);
  native_set_xmm_scalar(fp,2,oxygen);return 1;
 }
 case NS_NAH_HUD_WRITE:return hud_write(r,f);
 case NS_NAH_OXYGEN_HUD_SWITCH:
  if(!display_live(r)){native_hud_reset(r);return 0;}
  /* Only the observed display object may suppress the original seven-byte
   * store. Low oxygen still shows the main HUD through HUD_WRITE. */
  return f->rbx==r->hud.display&&r->view.firstpersonflag!=1;
 default:return 0;
 }
}
