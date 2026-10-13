#include "observer.h"
/* CT SHA-256 4154ee...686d, original CT L1182-1212. Offsets are hexadecimal.
   No runtime CT code is executed. Every attempt starts from the module root. */
typedef struct {u32 rva,offsets[OBS_MAX_LINKS-1],count,leaf,size;} Chain;
static const Chain chains[]={
 {0x8FB8018,{0xD8,0x18},3,0x30,12},
 {0x8FBB610,{0x20,0x88,0x10,0xA8},5,0x20,1},
 {0x8FAC380,{0x208,0x58,0x148,0x30,0x48},6,0x10,1}
};
/* Bound to ordinary Windows x64 user-space canonical addresses. Never probe
   kernel/noncanonical/null pages. RPM still owns the actual read decision. */
static int address(u64 base,u64 offset,usize size,u64*out){
 if(base<0x10000||base>0x7FFFFFFFFFFFULL||offset>0x7FFFFFFFFFFFULL-base)return 0;
 *out=base+offset;return size&&size-1<=0x7FFFFFFFFFFFULL-*out;
}
static void read_chain(ObsRead read,void*ctx,u64 base,const Chain*c,ObsState*state,u32 index,ObsValue*v){
 u64 values[OBS_MAX_LINKS]={0},locations[OBS_MAX_LINKS]={0},a=0;u8 bytes[12]={0};
 v->status=OBS_OK;
 for(u32 i=0;i<c->count;i++){
  v->fail_step=i;
  if(!address(i?values[i-1]:base,i?c->offsets[i-1]:c->rva,8,&a)){v->status=OBS_BAD_POINTER;goto failed;}
  locations[i]=a;
  if(!read(ctx,a,bytes,8)){v->status=OBS_READ_FAILED;goto failed;}
  values[i]=diag_u64(bytes);
  if(!values[i]){v->status=OBS_NULL;goto failed;}
  if(!address(values[i],0,1,&a)){v->status=OBS_BAD_POINTER;goto failed;}
 }
 v->fail_step=c->count;
 if(!address(values[c->count-1],c->leaf,c->size,&a)){v->status=OBS_BAD_POINTER;goto failed;}
 if(!read(ctx,a,bytes,c->size)){v->status=OBS_READ_FAILED;goto failed;}
 /* Recheck all links, not only the root. Detects some concurrent replacement,
    not ABA or lifetime/semantic identity. Payload itself is not frozen. */
 for(u32 j=c->count;j>0;j--){u64 now=0;u32 i=j-1;v->fail_step=i;
  if(!read(ctx,locations[i],&now,8)){v->status=OBS_READ_FAILED;goto failed;}
  if(now!=values[i]){v->status=OBS_CHANGED;goto failed;}
 }
 v->fail_step=c->count;
 if(c->size==12){
  for(u32 i=0;i<3;i++){u32 bits=diag_u32(bytes+4*i);float f;diag_copy(&f,&bits,4);
   if((bits&0x7F800000u)==0x7F800000u||f>1000000.0f||f< -1000000.0f){v->status=OBS_INVALID_VALUE;goto failed;}
  }
  diag_copy(v->xyz,bytes,12);
 }else{v->raw=bytes[0];if(v->raw>1){v->status=OBS_INVALID_VALUE;goto failed;}}
 if(!state->valid[index]||!diag_equal(state->links[index],values,sizeof values))state->generation[index]++;
 diag_copy(state->links[index],values,sizeof values);state->valid[index]=1;
 v->generation=state->generation[index];return;
failed:
 state->valid[index]=0;v->generation=state->generation[index];
}
void obs_sample(ObsRead read,void*ctx,u64 base,int supported,ObsState*state,ObsSample*out){
 diag_zero(out,sizeof *out);ObsValue*v[]={&out->player,&out->aim,&out->inventory,&out->interact};
 if(!supported){for(u32 i=0;i<4;i++)v[i]->status=OBS_UNSUPPORTED;for(u32 i=0;i<3;i++)state->valid[i]=0;return;}
 for(u32 i=0;i<3;i++)read_chain(read,ctx,base,&chains[i],state,i,v[i]);
 u64 a=0;u8 b[8];
 if(!address(base,0x8FB17F0,8,&a))out->interact.status=OBS_BAD_POINTER;
 else if(!read(ctx,a,b,8))out->interact.status=OBS_READ_FAILED;
 else out->interact.raw=diag_u64(b);
}
const char*obs_status_name(ObsStatus s){
 static const char*names[]={"READABLE_CANDIDATE","NULL_LINK","READ_FAILED","INVALID_POINTER","LINK_CHANGED","INVALID_VALUE","UNSUPPORTED_BUILD"};
 return s<=OBS_UNSUPPORTED?names[s]:"UNKNOWN";
}
