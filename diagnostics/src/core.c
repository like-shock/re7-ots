#include "core.h"
void diag_zero(void*p,usize n){u8*b=p;while(n--)*b++=0;}
void diag_copy(void*d,const void*s,usize n){u8*a=d;const u8*b=s;while(n--)*a++=*b++;}
int diag_equal(const void*a,const void*b,usize n){const u8*x=a,*y=b;while(n--)if(*x++!=*y++)return 0;return 1;}
u16 diag_u16(const u8*p){return (u16)(p[0]|((u16)p[1]<<8));}
u32 diag_u32(const u8*p){return (u32)p[0]|((u32)p[1]<<8)|((u32)p[2]<<16)|((u32)p[3]<<24);}
u64 diag_u64(const u8*p){return (u64)diag_u32(p)|((u64)diag_u32(p+4)<<32);}
static int nibble(char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;}
static int ws(char c){return c==' '||c=='\t'||c=='\r'||c=='\n';}
static int wild(char c){return c=='?'||c=='*';}
int diag_pattern(Pattern*p,const char*s){
 u32 run=0,start=0;diag_zero(p,sizeof *p);if(!s)return 0;
 while(*s){while(ws(*s))s++;if(!*s)break;const char*t=s;while(*s&&!ws(*s))s++;usize n=(usize)(s-t);u8 v=0,m=0;if(p->n==DIAG_MAX_PATTERN)return 0;
  if(n==1&&wild(t[0])){}else if(n==2){for(u32 k=0;k<2;k++){int q=nibble(t[k]);v<<=4;m<<=4;if(q>=0){v|=(u8)q;m|=15;}else if(!wild(t[k]))return 0;}}else return 0;
  p->bytes[p->n]=v;p->mask[p->n++]=m;
 }
 if(!p->n)return 0;
 for(u32 i=0;i<p->n;i++){if(p->mask[i]==255){if(!run)start=i;run++;if(run>p->anchor_n){p->anchor=start;p->anchor_n=run;}}else run=0;}
 if(!p->anchor_n)return 0;
 for(u32 i=0;i<256;i++)p->shift[i]=(u16)p->anchor_n;
 for(u32 i=0;i+1<p->anchor_n;i++)p->shift[p->bytes[p->anchor+i]]=(u16)(p->anchor_n-1-i);
 return 1;
}
void diag_scan(const u8*b,usize n,u32 base,usize old,const Pattern*p,Matches*m){
 if(!p->n||!p->anchor_n||n<p->n)return;
 usize pos=0,limit=n-p->n,alen=p->anchor_n,ao=p->anchor;
 while(pos<=limit){usize j=alen;while(j&&b[pos+ao+j-1]==p->bytes[ao+j-1])j--;
  if(!j){int same=1;for(u32 k=0;k<p->n;k++){if((b[pos+k]&p->mask[k])!=p->bytes[k]){same=0;break;}}
   if(same&&pos+p->n>old){u64 a=(u64)base+pos;if(a<=0xffffffffu){if(m->count<DIAG_MAX_MATCHES)m->rva[m->count]=(u32)a;if(m->count!=~(u64)0)m->count++;}}
   pos++; /* Keep overlapping occurrences. */
  }else pos+=p->shift[b[pos+ao+alen-1]];
 }
}
int diag_patch_rva(u32 a,int delta,u32 size,u32 n,u32*out){i64 v=(i64)a+(i64)delta;if(v<0||!n||(u64)v+n>size)return 0;*out=(u32)v;return 1;}
int diag_parse_pe(const u8*b,usize n,u64 file_size,PE*out){
 diag_zero(out,sizeof *out);if(n<64||b[0]!='M'||b[1]!='Z')return 0;
 u32 nt=diag_u32(b+60);if(nt<64||(u64)nt+24>n||diag_u32(b+nt)!=0x4550||diag_u16(b+nt+4)!=0x8664)return 0;
 u32 count=diag_u16(b+nt+6),opt=nt+24,optsz=diag_u16(b+nt+20);
 if(!count||count>DIAG_MAX_SECTIONS||optsz<112||(u64)opt+optsz+(u64)count*40>n||diag_u16(b+opt)!=0x20b)return 0;
 out->timestamp=diag_u32(b+nt+8);out->preferred_base=diag_u64(b+opt+24);out->image_size=diag_u32(b+opt+56);out->header_size=diag_u32(b+opt+60);out->checksum=diag_u32(b+opt+64);out->count=count;
 if(!out->image_size||out->image_size>DIAG_MAX_IMAGE||!out->header_size||out->header_size>out->image_size||(u64)opt+optsz+(u64)count*40>out->header_size)return 0;
 for(u32 i=0;i<count;i++){const u8*s=b+opt+optsz+i*40;Section*d=out->sections+i;diag_copy(d->name,s,8);d->name[8]=0;d->virtual_size=diag_u32(s+8);d->va=diag_u32(s+12);d->raw_size=diag_u32(s+16);d->raw=diag_u32(s+20);d->flags=diag_u32(s+36);
  u64 mapped=d->virtual_size>d->raw_size?d->virtual_size:d->raw_size;
  if((u64)d->va+mapped>out->image_size)return 0;
  if(file_size&&d->raw_size&&((u64)d->raw+d->raw_size>file_size||d->raw<out->header_size))return 0;
  for(u32 j=0;j<i;j++){Section*q=out->sections+j;u64 qs=q->virtual_size>q->raw_size?q->virtual_size:q->raw_size;if(mapped&&qs&&d->va<(u64)q->va+qs&&q->va<(u64)d->va+mapped)return 0;}
 }
 return 1;
}
int diag_rva_raw(const PE*p,u32 r,u32 n,u64*out){if(!n||(u64)r+n>p->image_size)return 0;if((u64)r+n<=p->header_size){*out=r;return 1;}for(u32 i=0;i<p->count;i++){const Section*s=p->sections+i;if(r>=s->va&&(u64)(r-s->va)+n<=s->raw_size){*out=(u64)s->raw+(r-s->va);return 1;}}return 0;}
static u32 ror(u32 x,u32 n){return (x>>n)|(x<<(32-n));}
static const u32 K[64]={
0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
static void sha_block(Sha256*s,const u8*b){u32 w[64],a=s->h[0],bb=s->h[1],c=s->h[2],d=s->h[3],e=s->h[4],f=s->h[5],g=s->h[6],h=s->h[7];for(u32 i=0;i<16;i++)w[i]=((u32)b[4*i]<<24)|((u32)b[4*i+1]<<16)|((u32)b[4*i+2]<<8)|b[4*i+3];for(u32 i=16;i<64;i++)w[i]=w[i-16]+(ror(w[i-15],7)^ror(w[i-15],18)^(w[i-15]>>3))+w[i-7]+(ror(w[i-2],17)^ror(w[i-2],19)^(w[i-2]>>10));for(u32 i=0;i<64;i++){u32 t1=h+(ror(e,6)^ror(e,11)^ror(e,25))+((e&f)^((~e)&g))+K[i]+w[i];u32 t2=(ror(a,2)^ror(a,13)^ror(a,22))+((a&bb)^(a&c)^(bb&c));h=g;g=f;f=e;e=d+t1;d=c;c=bb;bb=a;a=t1+t2;}s->h[0]+=a;s->h[1]+=bb;s->h[2]+=c;s->h[3]+=d;s->h[4]+=e;s->h[5]+=f;s->h[6]+=g;s->h[7]+=h;}
void diag_sha_init(Sha256*s){static const u32 h[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};diag_zero(s,sizeof *s);diag_copy(s->h,h,sizeof h);}
void diag_sha_update(Sha256*s,const u8*b,usize n){s->total+=n;while(n){usize k=64-s->used;if(k>n)k=n;diag_copy(s->block+s->used,b,k);s->used+=(u32)k;b+=k;n-=k;if(s->used==64){sha_block(s,s->block);s->used=0;}}}
void diag_sha_final(Sha256*s,u8 out[32]){u64 bits=s->total*8;s->block[s->used++]=0x80;if(s->used>56){while(s->used<64)s->block[s->used++]=0;sha_block(s,s->block);s->used=0;}while(s->used<56)s->block[s->used++]=0;for(u32 i=0;i<8;i++)s->block[63-i]=(u8)(bits>>(8*i));sha_block(s,s->block);for(u32 i=0;i<8;i++){out[4*i]=(u8)(s->h[i]>>24);out[4*i+1]=(u8)(s->h[i]>>16);out[4*i+2]=(u8)(s->h[i]>>8);out[4*i+3]=(u8)s->h[i];}}
