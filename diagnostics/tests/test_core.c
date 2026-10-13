#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core.h"
static int tests=0, failures=0;
#define CHECK(x) do { ++tests; if(!(x)){ ++failures; fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); }} while(0)
static void wr16(u8*p,u16 v){p[0]=v;p[1]=v>>8;}
static void wr32(u8*p,u32 v){p[0]=v;p[1]=v>>8;p[2]=v>>16;p[3]=v>>24;}
static void fixture(u8*b){memset(b,0,1024);b[0]='M';b[1]='Z';wr32(b+60,128);memcpy(b+128,"PE\0\0",4);wr16(b+132,0x8664);wr16(b+134,1);wr32(b+136,123);wr16(b+148,240);wr16(b+152,0x20b);wr32(b+152+56,0x4000);wr32(b+152+60,0x200);memcpy(b+392,".text",5);wr32(b+400,0x800);wr32(b+404,0x1000);wr32(b+408,0x600);wr32(b+412,0x200);wr32(b+428,0x60000020);}
int main(void){
 Pattern p; Matches m; u32 r; u64 off; PE pe; u8 h[1024],hash[32]; Sha256 sh; char hx[65];
 CHECK(diag_pattern(&p,"F3 0F ?? A? ?B * **"));
 {u8 b[]={0xf3,0x0f,0x20,0xa1,0x4b,0x22,0x88};diag_zero(&m,sizeof m);diag_scan(b,7,0x1000,0,&p,&m);CHECK(m.count==1 && m.rva[0]==0x1000);b[4]=0x4c;diag_zero(&m,sizeof m);diag_scan(b,7,0,0,&p,&m);CHECK(m.count==0);}
 CHECK(!diag_pattern(&p,"")); CHECK(!diag_pattern(&p,"GG")); CHECK(!diag_pattern(&p,"123"));CHECK(!diag_pattern(&p,"? ?? **"));
 CHECK(diag_pattern(&p,"AA AA"));{u8 b[]={0xaa,0xaa,0xaa};diag_zero(&m,sizeof m);diag_scan(b,3,10,0,&p,&m);CHECK(m.count==2 && m.rva[1]==11);}
 CHECK(diag_pattern(&p,"11 22 33"));{u8 b[]={0x11,0x22,0x33,0x11,0x22,0x33};diag_zero(&m,sizeof m);diag_scan(b,6,100,3,&p,&m);CHECK(m.count==1&&m.rva[0]==103);diag_zero(&m,sizeof m);diag_scan(b,4,100,2,&p,&m);CHECK(m.count==1&&m.rva[0]==100);}
 {u8 b[32];memset(b,0xaa,sizeof b);diag_pattern(&p,"AA");diag_zero(&m,sizeof m);diag_scan(b,32,0,0,&p,&m);CHECK(m.count==32 && m.rva[7]==7);}
 CHECK(diag_patch_rva(0x101a,-26,0x2000,31,&r)&&r==0x1000);CHECK(!diag_patch_rva(20,-26,0x2000,31,&r));CHECK(!diag_patch_rva(0x1ff0,0,0x2000,31,&r));CHECK(!diag_patch_rva(0xffffffffu,1,0x2000,1,&r));
 fixture(h);CHECK(diag_parse_pe(h,sizeof h,0x800,&pe));CHECK(pe.sections[0].va==0x1000&&pe.image_size==0x4000);CHECK(diag_rva_raw(&pe,0x1100,31,&off)&&off==0x300);CHECK(!diag_rva_raw(&pe,0x15ff,2,&off));CHECK(!diag_rva_raw(&pe,0x1700,1,&off));CHECK(!diag_rva_raw(&pe,0xfffffff0,40,&off));
 CHECK(!diag_parse_pe(h,100,0x800,&pe));h[0]=0;CHECK(!diag_parse_pe(h,sizeof h,0x800,&pe));fixture(h);wr32(h+60,0xfffffff0u);CHECK(!diag_parse_pe(h,sizeof h,0x800,&pe));fixture(h);wr32(h+408,0xffffffffu);CHECK(!diag_parse_pe(h,sizeof h,0x800,&pe));fixture(h);wr16(h+134,97);CHECK(!diag_parse_pe(h,sizeof h,0x800,&pe));
 diag_sha_init(&sh);diag_sha_final(&sh,hash);for(int i=0;i<32;i++)sprintf(hx+2*i,"%02x",hash[i]);CHECK(!strcmp(hx,"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
 diag_sha_init(&sh);diag_sha_update(&sh,(const u8*)"a",1);diag_sha_update(&sh,(const u8*)"bc",2);diag_sha_final(&sh,hash);for(int i=0;i<32;i++)sprintf(hx+2*i,"%02x",hash[i]);CHECK(!strcmp(hx,"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
 printf("core: %d checks, %d failures\n",tests,failures);return failures?1:0;
}
