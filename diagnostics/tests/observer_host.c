/* Executes production Windows wrapper with explicit fake OS. Not a DLL load. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "core.h"
static void host_sha_final(Sha256*s,u8*out);
#define diag_sha_final host_sha_final
#include "../src/observer_plugin.c"
#undef diag_sha_final
static char native_out[20000000],status_out[BUF_CAP+1];static u32 native_n,status_n;static int sleeps,mode,thread_called,validation_reads;static short key;static u64 ticks=1000;static void(*callback)(void);static u8 header_bytes[1024];
static void host_sha_final(Sha256*s,u8*out){diag_sha_final(s,out);if(mode!=1){for(int i=0;i<32;i++){unsigned x;sscanf(OBS_EXE_HASH+2*i,"%2x",&x);out[i]=(u8)x;}}}
static void set32(int p,u32 n){memcpy(header_bytes+p,&n,4);}static void set16(int p,u16 n){memcpy(header_bytes+p,&n,2);}
HMODULE GetModuleHandleW(const WCHAR*w){(void)w;return (void*)0x140000000ULL;}
u32 GetModuleFileNameW(HMODULE h,WCHAR*w,u32 n){(void)h;(void)n;const char*s="C:\\Games\\re7.exe";u32 i=0;do{w[i]=(u8)s[i];}while(s[i++]);return i-1;}
BOOL GetModuleHandleExW(u32 f,const WCHAR*a,HMODULE*h){(void)a;assert(f==5);*h=(void*)1;return 1;}
HANDLE GetCurrentProcess(void){return (void*)1;}HANDLE GetCurrentThread(void){return (void*)2;}u32 GetCurrentProcessId(void){return 42;}BOOL SetThreadPriority(HANDLE h,int p){(void)h;(void)p;return 1;}
BOOL ReadProcessMemory(HANDLE h,const void*a,void*b,usize n,usize*got){(void)h;u64 r=(u64)(usize)a-0x140000000ULL;*got=0;if(r<sizeof header_bytes&&n<=sizeof header_bytes-r){memcpy(b,header_bytes+r,n);*got=n;return 1;}if(mode==2)return 0;if(r==0x8FB8018||r==0x8FBB610||r==0x8FAC380||r==0x8FB17F0){memset(b,0,n);*got=n;return 1;}return 0;}
BOOL CreateDirectoryW(const WCHAR*w,void*p){(void)w;(void)p;return 1;}
HANDLE CreateFileW(const WCHAR*w,u32 access,u32 share,void*sa,u32 disposition,u32 flags,HANDLE t){(void)w;(void)share;(void)sa;(void)flags;(void)t;if(access==GENERIC_READ)return (void*)3;if(disposition==CREATE_NEW)return (void*)4;status_n=0;return (void*)5;}
BOOL ReadFile(HANDLE h,void*b,u32 n,u32*got,void*o){(void)h;(void)o;assert(n<=sizeof header_bytes);memcpy(b,header_bytes,n);*got=n;validation_reads++;if(mode==10)stop_requested=1;return 1;}
BOOL WriteFile(HANDLE h,const void*b,u32 n,u32*put,void*o){(void)o;if(mode==3){*put=0;return 0;}if(mode==4&&n>7)n=7;if(h==(void*)4){assert(native_n+n<sizeof native_out);memcpy(native_out+native_n,b,n);native_n+=n;native_out[native_n]=0;}else{assert(status_n+n<=BUF_CAP);memcpy(status_out+status_n,b,n);status_n+=n;status_out[status_n]=0;}*put=n;return 1;}
BOOL CloseHandle(HANDLE h){(void)h;return 1;}BOOL FlushFileBuffers(HANDLE h){(void)h;return 1;}BOOL GetFileSizeEx(HANDLE h,LARGE_INTEGER*n){(void)h;n->QuadPart=sizeof header_bytes;return 1;}
void GetLocalTime(SYSTEMTIME*t){memset(t,0,sizeof *t);t->year=2026;t->month=10;t->day=13;}
u64 GetTickCount64(void){return ticks;}u32 GetLastError(void){return 0;}
HANDLE CreateThread(void*a,usize st,u32(*f)(void*),void*p,u32 fl,u32*id){(void)a;(void)st;(void)fl;(void)id;thread_called++;if(mode==8)return 0;f(p);return (void*)6;}
short GetAsyncKeyState(int k){return k==0x77?key:0;}void*GetForegroundWindow(void){return (void*)7;}u32 GetWindowThreadProcessId(void*w,u32*p){(void)w;*p=mode==9?99:42;return 1;}
void Sleep(u32 ms){sleeps++;ticks+=ms;if(sleeps==1)stage=1;if(mode==5)ticks+=OBS_MAX_MS;if(mode==7)bytes_written=OBS_MAX_BYTES-BUF_CAP;if(mode!=6&&sleeps>=3)stop_requested=1;}
static _Bool reg(void(*f)(void)){callback=f;return 1;}static void msg(const char*f,...){(void)f;}
int main(int argc,char**argv){assert(argc==2);const char*names[]={"normal","hash_mismatch","read_fail","write_fail","partial_write","time_limit","sample_limit","size_limit","thread_fail","focus","stopped_validation","session_limit"};for(int i=0;i<12;i++)if(!strcmp(argv[1],names[i]))mode=i;
 memset(header_bytes,0,sizeof header_bytes);memcpy(header_bytes,"MZ",2);set32(60,128);memcpy(header_bytes+128,"PE\0\0",4);set16(132,0x8664);set16(134,1);set32(136,OBS_TIMESTAMP);set16(148,240);set16(152,0x20b);set32(208,OBS_IMAGE_SIZE);set32(212,1024);memcpy(header_bytes+392,".text",5);set32(400,0x1000);set32(404,0x1000);set32(408,0x200);set32(412,0x400);
 REFFunctions fn={0};fn.on_present=reg;fn.log_info=msg;fn.log_error=msg;REFInit init={0,0,&fn,0,0};assert(reframework_plugin_initialize(&init));assert(callback);key=(short)0x8000;callback();
 if(mode==8){assert(thread_called==1&&busy==0&&sessions==0);return 0;}if(mode==9){assert(thread_called==0);return 0;}
 if(mode==1){assert(strstr(native_out,"UNSUPPORTED_EXE_HASH")&&!strstr(native_out,"\"kind\":\"sample\""));}
 else if(mode==3){assert(native_n==0&&busy==0);}
 else if(mode==2){assert(strstr(native_out,"READ_FAILED"));}
 else if(mode==5||mode==6||mode==7){assert(strstr(status_out,"\"phase\":\"LIMIT\""));if(mode==6)assert(sleeps==9000);}
 else {assert(strstr(status_out,"\"phase\":\"STOPPED\""));if(mode==10)assert(strstr(status_out,"\"seq\":0"));else assert(strstr(native_out,"\"stage\":\"MOVE\""));}
 if(mode==11){for(int i=0;i<5;i++){key=0;callback();key=(short)0x8000;callback();}assert(thread_called==4&&sessions==4);}
 assert(mode==3 ? validation_reads==0 : validation_reads>=1);if(mode!=3)puts(status_out);return 0;
}
