/* Production wrapper with fake Win32. SHA substitution is test-only. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core.h"
static void host_sha_final(Sha256*,u8*);
#define diag_sha_final host_sha_final
#include "../src/explore_plugin.c"
#undef diag_sha_final
static int mode,sleeps,threads,fixed_reads,hash_reads,moves,rename_failures;
static u32 host_error;
static u64 ticks=1000;
static u64 last_publish_tick,max_publish_gap;
static u8 pe_bytes[1024];
static char temp_out[8193],published[8193],sample_out[8193];
static u32 temp_n;
static void host_sha_final(Sha256*s,u8*out){diag_sha_final(s,out);if(mode!=1)for(int i=0;i<32;i++){unsigned x;sscanf(OBS_EXE_HASH+2*i,"%2x",&x);out[i]=(u8)x;}}
static void set32(int p,u32 x){memcpy(pe_bytes+p,&x,4);}static void set16(int p,u16 x){memcpy(pe_bytes+p,&x,2);}
HMODULE GetModuleHandleW(const WCHAR*p){(void)p;return (void*)0x140000000ULL;}
u32 GetModuleFileNameW(HMODULE h,WCHAR*p,u32 n){(void)h;(void)n;const char*s=mode==9?"C:\\Games\\other.exe":"C:\\Games\\re7.exe";u32 i=0;do{p[i]=(u8)s[i];}while(s[i++]);return i-1;}
BOOL GetModuleHandleExW(u32 f,const WCHAR*p,HMODULE*h){(void)p;assert(f==5);*h=(void*)1;return 1;}
HANDLE GetCurrentProcess(void){return (void*)1;}HANDLE GetCurrentThread(void){return (void*)2;}
u32 GetCurrentProcessId(void){return 42;}BOOL SetThreadPriority(HANDLE h,int p){(void)h;(void)p;return 1;}
BOOL ReadProcessMemory(HANDLE h,const void*a,void*b,usize n,usize*got){(void)h;u64 r=(u64)(usize)a-0x140000000ULL;*got=0;if(r<sizeof pe_bytes&&n<=sizeof pe_bytes-r){memcpy(b,pe_bytes+r,n);*got=n;return 1;}fixed_reads++;if(mode==2)return 0;if(r==0x8FB8018||r==0x8FBB610||r==0x8FAC380||r==0x8FB17F0){memset(b,0,n);*got=n;return 1;}return 0;}
BOOL CreateDirectoryW(const WCHAR*p,void*a){(void)p;(void)a;return 1;}
HANDLE CreateFileW(const WCHAR*p,u32 access,u32 share,void*a,u32 d,u32 flags,HANDLE t){(void)p;(void)share;(void)a;(void)d;(void)flags;(void)t;if(access==GENERIC_READ)return (void*)3;temp_n=0;return (void*)4;}
BOOL ReadFile(HANDLE h,void*b,u32 n,u32*got,void*a){(void)h;(void)a;assert(n<=sizeof pe_bytes);memcpy(b,pe_bytes,n);*got=n;hash_reads++;return 1;}
BOOL WriteFile(HANDLE h,const void*b,u32 n,u32*got,void*a){(void)h;(void)a;if(mode==3){*got=0;host_error=1117;return 0;}if(mode==4&&n>7)n=7;assert(temp_n+n<sizeof temp_out);memcpy(temp_out+temp_n,b,n);temp_n+=n;temp_out[temp_n]=0;*got=n;return 1;}
BOOL MoveFileExW(const WCHAR*a,const WCHAR*b,u32 flags){(void)a;(void)b;assert(flags==1);if(mode==5){host_error=32;return 0;}if(mode==12&&(sleeps==9||sleeps==10)){rename_failures++;host_error=32;return 0;}if(((mode==10&&moves==2)||(mode==11&&moves==0))&&rename_failures++==0){host_error=32;return 0;}memcpy(published,temp_out,temp_n+1);moves++;if(strstr(published,"\"phase\":\"MONITORING\"")){memcpy(sample_out,published,temp_n+1);if(last_publish_tick&&ticks-last_publish_tick>max_publish_gap)max_publish_gap=ticks-last_publish_tick;last_publish_tick=ticks;}return 1;}
BOOL CloseHandle(HANDLE h){(void)h;host_error=0;return 1;}
BOOL GetFileSizeEx(HANDLE h,LARGE_INTEGER*n){(void)h;n->QuadPart=sizeof pe_bytes;return 1;}
void GetLocalTime(SYSTEMTIME*p){memset(p,0,sizeof *p);p->year=2026;p->month=10;p->day=13;}
u64 GetTickCount64(void){return ticks;}u32 GetLastError(void){return host_error;}
void Sleep(u32 n){assert(n==50||n==250);sleeps++;ticks+=n;if(sleeps==(mode==12?20:3))ticks+=43200000;}
void* GetForegroundWindow(void){return (void*)7;}u32 GetWindowThreadProcessId(void*w,u32*p){(void)w;*p=mode==7?99:42;return 1;}
HANDLE CreateThread(void*a,usize s,u32(*f)(void*),void*p,u32 fl,u32*id){(void)a;(void)s;(void)fl;(void)id;threads++;if(mode==6)return 0;f(p);return (void*)6;}
static void msg(const char*s,...){(void)s;}
int main(int argc,char**argv){assert(argc==2);const char*names[]={"normal","hash","read","write","short","rename","thread","focus","pe","exe","rename_once","initial_rename_once","transient_contention"};for(int i=0;i<13;i++)if(!strcmp(argv[1],names[i]))mode=i;
 memcpy(pe_bytes,"MZ",2);set32(60,128);memcpy(pe_bytes+128,"PE\0\0",4);set16(132,0x8664);set16(134,1);set32(136,mode==8?0:OBS_TIMESTAMP);set16(148,240);set16(152,0x20b);set32(208,OBS_IMAGE_SIZE);set32(212,1024);memcpy(pe_bytes+392,".text",5);set32(400,0x1000);set32(404,0x1000);set32(408,0x200);set32(412,0x400);
 REFFunctions f={0};f.log_info=msg;f.log_error=msg;REFInit init={0,0,&f,0,0};int ok=reframework_plugin_initialize(&init);
 if(mode==6||mode==9){assert(!ok);assert(fixed_reads==0);return 0;}assert(ok&&threads==1);
 if(mode==1||mode==8){assert(fixed_reads==0);assert(strstr(published,"UNSUPPORTED"));if(mode==8)assert(hash_reads==0);return 0;}
 if(mode==3||mode==5){assert(fixed_reads==12&&moves==0&&sleeps==3);Buffer evidence;common(&evidence,"TEST",0,ticks);assert(strstr(evidence.data,"\"failures\":5"));assert(strstr(evidence.data,mode==3?"\"last_error\":1117":"\"last_error\":32"));return 0;}
 if(mode==10||mode==11){assert(rename_failures>=1&&moves>=3);}
 if(mode==12){fprintf(stderr,"successful-publication gap: %llu ms\n",(unsigned long long)max_publish_gap);assert(rename_failures==2);assert(max_publish_gap<=150);assert(strstr(sample_out,"\"publication\":{\"failures\":2,\"last_error\":32}"));assert(strstr(sample_out,"\"monitor_version\":\"0.3.1\""));return 0;}
 assert(sleeps==3&&fixed_reads==12);assert(strstr(published,"\"phase\":\"LIMIT\""));assert(strstr(sample_out,"\"seq\":3"));
 assert(strstr(sample_out,mode==7?"\"foreground\":false":"\"foreground\":true"));
 assert(strstr(sample_out,mode==2?"READ_FAILED":"NULL_LINK"));
 assert(reframework_plugin_initialize(&init));assert(threads==1);
 puts("automatic monitor scenario PASS");return 0;
}
