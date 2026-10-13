/* Synthetic Win32 backend for the *same* plugin source. This is NOT Wine/Proton. */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include "win_min.h"
#include "ref_min.h"
static u8* image;static usize image_n;static char exe_path[2048];static const char* scenario;static u32 last_error;static short key;static void(*present)(void);static u64 ticks;
extern _Bool reframework_plugin_initialize(const REFInit*);
extern void reframework_plugin_required_version(REFVersion*);
static void narrow(const WCHAR*w,char*c){usize n=0;while(w[n]&&n<2047){c[n]=w[n]=='\\'?'/':(char)w[n];n++;}c[n]=0;}
HMODULE GetModuleHandleW(const WCHAR*w){(void)w;return image;}
u32 GetModuleFileNameW(HMODULE h,WCHAR*w,u32 cap){(void)h;usize n=strlen(exe_path);if(n+1>cap)return cap;for(usize i=0;i<=n;i++)w[i]=(WCHAR)(u8)exe_path[i];return(u32)n;}
BOOL GetModuleHandleExW(u32 f,const WCHAR*a,HMODULE*h){(void)a;if(f!=5)return 0;*h=image;return 1;}
HANDLE GetCurrentProcess(void){return(HANDLE)3;}HANDLE GetCurrentThread(void){return(HANDLE)4;}BOOL SetThreadPriority(HANDLE t,int p){(void)t;(void)p;return 1;}
BOOL ReadProcessMemory(HANDLE h,const void*a,void*b,usize n,usize*got){(void)h;*got=0;usize x=(usize)a,base=(usize)image;if(x<base||x-base>image_n||n>image_n-(x-base))return 0;usize r=x-base;
 if(r<0x10000&&r+n>0x1000)return 0; /* real inaccessible gap after headers */
 if(!strcmp(scenario,"unreadable")&&r<0x22000&&r+n>0x21000)return 0;
 memcpy(b,a,n);*got=n;return 1;}
usize VirtualQuery(const void*a,MEMORY_BASIC_INFORMATION*m,usize n){if(n!=sizeof *m)return 0;usize x=(usize)a,base=(usize)image;if(x<base||x-base>=image_n)return 0;usize r=x-base;memset(m,0,sizeof *m);m->AllocationBase=image;m->State=MEM_COMMIT;m->Protect=0x20;usize start=0,end=0x1000;
 if(r>=0x1000&&r<0x10000){start=0x1000;end=0x10000;m->Protect=1;}else if(r>=0x10000){start=0x10000;end=image_n;}m->BaseAddress=image+start;m->RegionSize=end-start;return sizeof *m;}
HANDLE GetProcessHeap(void){return(HANDLE)5;}void*HeapAlloc(HANDLE h,u32 f,usize n){(void)h;(void)f;return malloc(n);}BOOL HeapFree(HANDLE h,u32 f,void*p){(void)h;(void)f;free(p);return 1;}
HANDLE CreateFileW(const WCHAR*w,u32 access,u32 share,void*sa,u32 disposition,u32 flags,HANDLE tpl){(void)share;(void)sa;(void)flags;(void)tpl;char p[2048];narrow(w,p);if(access==GENERIC_READ&&!strcmp(scenario,"file_denied")){last_error=5;return INVALID_HANDLE;}if(access==GENERIC_WRITE&&!strcmp(scenario,"log_denied")){last_error=5;return INVALID_HANDLE;}FILE*f=fopen(p,access==GENERIC_READ?"rb":disposition==CREATE_NEW?"wbx":"wb");if(!f){last_error=5;return INVALID_HANDLE;}return(HANDLE)f;}
BOOL CreateDirectoryW(const WCHAR*w,void*s){(void)s;char p[2048];narrow(w,p);if(!mkdir(p,0700))return 1;last_error=errno==EEXIST?183:5;return 0;}
BOOL ReadFile(HANDLE f,void*b,u32 n,u32*got,void*o){(void)o;*got=(u32)fread(b,1,n,(FILE*)f);return !ferror((FILE*)f);}
BOOL WriteFile(HANDLE f,const void*b,u32 n,u32*put,void*o){(void)o;if(!strcmp(scenario,"log_write_fail")){*put=0;last_error=5;return 0;}*put=(u32)fwrite(b,1,n,(FILE*)f);return !ferror((FILE*)f);}
BOOL CloseHandle(HANDLE h){if(h==(HANDLE)2)return 1;return fclose((FILE*)h)==0;}BOOL FlushFileBuffers(HANDLE h){return fflush((FILE*)h)==0;}
BOOL GetFileSizeEx(HANDLE h,LARGE_INTEGER*s){long p=ftell((FILE*)h);if(fseek((FILE*)h,0,SEEK_END))return 0;s->QuadPart=ftell((FILE*)h);return fseek((FILE*)h,p,SEEK_SET)==0;}
BOOL SetFilePointerEx(HANDLE h,LARGE_INTEGER p,LARGE_INTEGER*out,u32 method){(void)out;(void)method;return fseek((FILE*)h,(long)p.QuadPart,SEEK_SET)==0;}
void GetLocalTime(SYSTEMTIME*s){memset(s,0,sizeof *s);s->year=2026;s->month=10;s->day=12;s->hour=12;}
u64 GetTickCount64(void){return ++ticks;}u32 GetLastError(void){return last_error;}void OutputDebugStringA(const char*s){fprintf(stderr,"DEBUG: %s\n",s);}
HANDLE CreateThread(void*a,usize stack,u32(*fn)(void*),void*p,u32 flags,u32*id){(void)a;(void)stack;(void)flags;(void)id;fn(p);return(HANDLE)2;}
short GetAsyncKeyState(int k){return k==0x78?key:0;}
static _Bool reg(void(*p)(void)){present=p;return 1;}static void logmsg(const char*f,...){va_list ap;va_start(ap,f);vfprintf(stdout,f,ap);puts("");va_end(ap);}
int main(int argc,char**argv){if(argc!=4)return 2;snprintf(exe_path,sizeof exe_path,"%s/re7.exe",argv[1]);scenario=argv[2];FILE*f=fopen(argv[3],"rb");if(!f)return 3;fseek(f,0,SEEK_END);image_n=(usize)ftell(f);rewind(f);image=aligned_alloc(4096,image_n);if(!image)return 4;if(fread(image,1,image_n,f)!=image_n)return 5;fclose(f);
 u8*before=malloc(image_n);memcpy(before,image,image_n);REFFunctions funcs={0};funcs.on_present=reg;funcs.log_info=logmsg;funcs.log_error=logmsg;REFVersion v={1,15,0,"RE7"};REFInit init={0,&v,&funcs,0,0};
 REFVersion required={0};reframework_plugin_required_version(&required);if(required.major!=1||strcmp(required.game_name,"RE7"))return 6;
 if(!reframework_plugin_initialize(&init)||!present)return 7;for(int i=0;i<60;i++)present();
 if(!strcmp(scenario,"repeat")){scenario="file_denied";key=(short)0x8000;present();key=0;present();}
 if(memcmp(before,image,image_n)){fprintf(stderr,"GAME MEMORY MODIFIED\n");return 8;}free(before);free(image);return 0;
}
