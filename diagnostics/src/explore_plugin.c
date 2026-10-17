/* Read-only automatic native monitor. Camera writes live only in Lua callbacks. */
#include "win_min.h"
#include "ref_min.h"
#include "observer.h"
#include "native_lua.h"
#include <stdatomic.h>
#ifdef DIAG_HOST_TEST
#define EXPORT
#else
#define EXPORT __declspec(dllexport)
int _fltused=0;
#endif
WIN_IMPORT void WINAPI Sleep(u32);
WIN_IMPORT void* WINAPI GetForegroundWindow(void);
WIN_IMPORT u32 WINAPI GetWindowThreadProcessId(void*,u32*);
WIN_IMPORT u32 WINAPI GetCurrentProcessId(void);
WIN_IMPORT BOOL WINAPI MoveFileExW(const WCHAR*,const WCHAR*,u32);
#define PATH_CAP 32768u
#define BUF_CAP 8192u
#define EXP_VERSION "0.4.0"
#define EXP_PROTOCOL "0.3.0"
#define EXP_MAX_MS 43200000ULL
static const REFFunctions*ref;
static int initialized;
static _Atomic u32 publish_failures,last_publish_error;
/* Release/acquire publishes immutable metadata. Each consumer owns its ObsState. */
static _Atomic int native_phase; /* 0 validating, 1 monitoring, 2 unsupported, 3 limit */
static u64 validated_base,validated_start;
static const char *validation_error;
static atomic_flag direct_lock=ATOMIC_FLAG_INIT;
static char marker,session[96];
static WCHAR exe_path[PATH_CAP],folder[PATH_CAP],status_path[PATH_CAP],temp_path[PATH_CAP];
static u8 hash_buffer[1024*1024],pe_header[65536];
typedef struct {char data[BUF_CAP];u32 n;int failed;} Buffer;
static void add(Buffer*b,const char*s){while(*s){if(b->n>=BUF_CAP){b->failed=1;return;}b->data[b->n++]=*s++;}}
static void number(Buffer*b,u64 n){char tmp[32];u32 k=0;do{tmp[k++]=(char)('0'+n%10);n/=10;}while(n);while(k){char c[2]={tmp[--k],0};add(b,c);}}
static void quote(Buffer*b,const char*s){add(b,"\"");add(b,s);add(b,"\"");} /* Only internal literals/numeric session identifiers. */
static void hex(Buffer*b,u64 n){char tmp[16];u32 k=0;add(b,"\"0x");do{tmp[k++]="0123456789ABCDEF"[n&15];n>>=4;}while(n);while(k){char c[2]={tmp[--k],0};add(b,c);}add(b,"\"");}
static void decimal(Buffer*b,float f){double d=f;if(d<0){add(b,"-");d=-d;}u64 scaled=(u64)(d*1000000.0+0.5);number(b,scaled/1000000);add(b,".");u64 rem=scaled%1000000;u32 div=100000;while(div){char c[2]={(char)('0'+rem/div),0};add(b,c);rem%=div;div/=10;}}
static void message(int error,const char*s){if(ref){if(error&&ref->log_error)ref->log_error("[RE7_EXPLORE] %s",s);else if(ref->log_info)ref->log_info("[RE7_EXPLORE] %s",s);}}
static int append(WCHAR*p,u32*n,const char*s){while(*s){if(*n+1>=PATH_CAP)return 0;p[(*n)++]=(WCHAR)(u8)*s++;}p[*n]=0;return 1;}
static int read_memory(void*ctx,u64 a,void*b,usize n){(void)ctx;usize got=0;return n&&a<=~(u64)0-n&&ReadProcessMemory(GetCurrentProcess(),(void*)(usize)a,b,n,&got)&&got==n;}
static int write_all(HANDLE h,const Buffer*b){if(b->failed)return 0;u32 done=0;while(done<b->n){u32 n=0;if(!WriteFile(h,b->data+done,b->n-done,&n,0)||!n||n>b->n-done)return 0;done+=n;}return 1;}
static void value(Buffer*b,const char*name,const ObsValue*v,int xyz){add(b,",\"");add(b,name);add(b,"\":{\"status\":");quote(b,obs_status_name(v->status));add(b,",\"fail_step\":");if(v->status==OBS_OK)add(b,"null");else number(b,v->fail_step);add(b,",\"generation\":");number(b,v->generation);
 if(xyz){add(b,",\"xyz\":");if(v->status==OBS_OK){add(b,"[");for(u32 i=0;i<3;i++){if(i)add(b,",");decimal(b,v->xyz[i]);}add(b,"]");}else add(b,"null");}
 else{add(b,",\"raw\":");if(v->status==OBS_OK||v->status==OBS_INVALID_VALUE)number(b,v->raw);else add(b,"null");}add(b,"}");
}
static const char*validate_build(u64 base,char digest_text[65]){
 if(!read_memory(0,base,pe_header,64)||pe_header[0]!='M'||pe_header[1]!='Z')return "PE_READ_FAILED";
 u32 nt=diag_u32(pe_header+60);if(nt<64||nt>sizeof pe_header-24||!read_memory(0,base+nt,pe_header+nt,24))return "PE_READ_FAILED";
 u64 n=(u64)nt+24+diag_u16(pe_header+nt+20)+(u64)diag_u16(pe_header+nt+6)*40;
 if(n>sizeof pe_header||!read_memory(0,base,pe_header,(usize)n))return "PE_READ_FAILED";
 PE pe;if(!diag_parse_pe(pe_header,(usize)n,0,&pe)||pe.timestamp!=OBS_TIMESTAMP||pe.image_size!=OBS_IMAGE_SIZE)return "UNSUPPORTED_PE";
 HANDLE f=CreateFileW(exe_path,GENERIC_READ,7,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);if(f==INVALID_HANDLE)return "EXE_OPEN_FAILED";
 LARGE_INTEGER size;const char*error=0;Sha256 hash;diag_sha_init(&hash);
 if(!GetFileSizeEx(f,&size)||size.QuadPart<=0||size.QuadPart>DIAG_MAX_IMAGE)error="EXE_SIZE_INVALID";
 u64 done=0;
 while(!error&&done<(u64)size.QuadPart){
  u64 left=(u64)size.QuadPart-done;u32 want=left>sizeof hash_buffer?sizeof hash_buffer:(u32)left,got=0;
  if(!ReadFile(f,hash_buffer,want,&got,0)||!got||got>want){error="EXE_READ_FAILED";break;}
  diag_sha_update(&hash,hash_buffer,got);done+=got;
 }
 CloseHandle(f);if(error)return error;
 u8 digest[32];diag_sha_final(&hash,digest);for(u32 i=0;i<32;i++){digest_text[2*i]="0123456789abcdef"[digest[i]>>4];digest_text[2*i+1]="0123456789abcdef"[digest[i]&15];}digest_text[64]=0;
 return diag_equal(digest_text,OBS_EXE_HASH,64)?0:"UNSUPPORTED_EXE_HASH";
}

static int setup_paths(void){
 u32 n=GetModuleFileNameW(0,exe_path,PATH_CAP);if(!n||n>=PATH_CAP)return 0;
 u32 start=n;while(start&&exe_path[start-1]!='\\'&&exe_path[start-1]!='/')start--;
 const char*want="re7.exe";if(n-start!=7)return 0;
 for(u32 i=0;i<7;i++){u16 c=exe_path[start+i];if(c>='A'&&c<='Z')c+=32;if(c!=(u16)want[i])return 0;}
 diag_copy(folder,exe_path,start*2);n=start;
 const char*dirs[]={"reframework","\\data","\\RE7_OTS_Explore"};
 for(u32 i=0;i<3;i++){if(!append(folder,&n,dirs[i]))return 0;if(!CreateDirectoryW(folder,0)&&GetLastError()!=183)return 0;}
 u32 m=n;diag_copy(status_path,folder,(n+1)*2);diag_copy(temp_path,folder,(n+1)*2);
 return append(status_path,&n,"\\status.json")&&append(temp_path,&m,"\\status.tmp");
}
static int publish(const Buffer*b){
 HANDLE f=CreateFileW(temp_path,GENERIC_WRITE,7,0,2,FILE_ATTRIBUTE_NORMAL,0);
 if(f==INVALID_HANDLE){last_publish_error=GetLastError();publish_failures++;return 0;}
 int ok=write_all(f,b);u32 error=ok?0:GetLastError();
 if(!CloseHandle(f)){if(ok)error=GetLastError();ok=0;}
 /* Readers see either complete snapshot. Failure leaves the previous heartbeat stale. */
 if(ok&&!MoveFileExW(temp_path,status_path,1)){error=GetLastError();ok=0;}
 if(!ok){publish_failures++;last_publish_error=error;}
 return ok;
}
static int foreground(void){u32 pid=0;void*w=GetForegroundWindow();if(w)GetWindowThreadProcessId(w,&pid);return pid==GetCurrentProcessId();}
static void common(Buffer*b,const char*phase,u32 seq,u64 tick){
 diag_zero(b,sizeof *b);add(b,"{\"schema\":1,\"version\":\"" EXP_PROTOCOL "\",\"monitor_version\":\"" EXP_VERSION "\",\"session\":");quote(b,session);
 add(b,",\"phase\":");quote(b,phase);add(b,",\"seq\":");number(b,seq);add(b,",\"tick_ms\":");number(b,tick);
 add(b,",\"foreground\":");add(b,foreground()?"true":"false");
 /* Completed attempts only: this snapshot's own publication is not known yet. */
 add(b,",\"publication\":{\"failures\":");number(b,publish_failures);
 add(b,",\"last_error\":");number(b,last_publish_error);add(b,"}");
}
static void sample_json(Buffer*b,const ObsSample*s,u32 seq,u64 tick){
  common(b,"MONITORING",seq,tick);add(b,",\"build\":\"KNOWN_EXE_HASH_AND_PE\"");
  value(b,"player",&s->player,1);value(b,"aim",&s->aim,0);value(b,"inventory",&s->inventory,0);
  add(b,",\"interact\":{\"status\":");quote(b,obs_status_name(s->interact.status));add(b,",\"slot_hex\":");if(s->interact.status==OBS_OK)hex(b,s->interact.raw);else add(b,"null");
  add(b,",\"ct_low16_nonzero\":");add(b,s->interact.status!=OBS_OK?"null":(s->interact.raw&0xFFFF)?"true":"false");
  add(b,",\"ct_low32_nonzero\":");add(b,s->interact.status!=OBS_OK?"null":(s->interact.raw&0xFFFFFFFF)?"true":"false");add(b,"}}\n");
}
double native_clock(void){return (double)GetTickCount64()/1000.0;}
size_t native_read_snapshot(char *out,size_t cap){
 if(!out||cap<BUF_CAP||atomic_flag_test_and_set_explicit(&direct_lock,memory_order_acquire))return 0;
 static ObsState state;static Buffer cached;static u64 previous_tick;static u32 seq;
 static int previous_phase=-1;
 u64 tick=GetTickCount64();int phase=atomic_load_explicit(&native_phase,memory_order_acquire);
 if(phase==1&&tick-validated_start>=EXP_MAX_MS)phase=3;
 if(phase!=previous_phase||tick!=previous_tick){
  if(phase==1){ObsSample sample;obs_sample(read_memory,0,validated_base,1,&state,&sample);sample_json(&cached,&sample,++seq,tick);}
  else {common(&cached,phase==0?"VALIDATING":phase==2?"UNSUPPORTED":"LIMIT",seq,tick);
   if(phase==2){add(&cached,",\"reason\":");quote(&cached,validation_error);}
   if(phase==3)add(&cached,",\"reason\":\"TWELVE_HOUR_MONITOR_LIMIT\"");
   add(&cached,"}\n");}
  previous_tick=tick;previous_phase=phase;
 }
 size_t n=cached.failed?0:cached.n;if(n)diag_copy(out,cached.data,n);
 atomic_flag_clear_explicit(&direct_lock,memory_order_release);return n;
}
static void lua_created(void *state){
 /* Follow the official example's Lua lock; no sampling lock spans any Lua API. */
 int locked=ref->lock_lua&&ref->unlock_lua;if(locked)ref->lock_lua();
 int ok=native_lua_register(state);
 if(locked)ref->unlock_lua();
 if(!ok)message(1,"Native Lua API unavailable; requires compatible Lua 5.4");
}
static int setup_session(void){
 Buffer b;SYSTEMTIME t;GetLocalTime(&t);diag_zero(&b,sizeof b);add(&b,"re7_explore_");
 number(&b,t.year);add(&b,"_");number(&b,t.month);add(&b,"_");number(&b,t.day);add(&b,"_");number(&b,GetTickCount64());add(&b,"_");number(&b,GetCurrentProcessId());
 if(b.failed||b.n>=sizeof session)return 0;
 diag_copy(session,b.data,b.n);session[b.n]=0;return 1;
}
static u32 WINAPI monitor(void*unused){
 (void)unused;SetThreadPriority(GetCurrentThread(),-1);
 static Buffer b;
 common(&b,"VALIDATING",0,GetTickCount64());add(&b,"}\n");int publication_failed=!publish(&b);
 if(publication_failed)message(1,"Initial status publication failed; monitor will retry");
 char digest[65]={0};u64 base=(u64)(usize)GetModuleHandleW(0);const char*error=validate_build(base,digest);
 if(error){validation_error=error;atomic_store_explicit(&native_phase,2,memory_order_release);common(&b,"UNSUPPORTED",0,GetTickCount64());add(&b,",\"reason\":");quote(&b,error);add(&b,"}\n");if(!publish(&b))message(1,"Unsupported-build status publication failed");return 0;}
 ObsState state;diag_zero(&state,sizeof state);u64 start=GetTickCount64();u32 seq=0;
 validated_base=base;validated_start=start;atomic_store_explicit(&native_phase,1,memory_order_release);
 while(GetTickCount64()-start<EXP_MAX_MS){
  ObsSample s;obs_sample(read_memory,0,base,1,&state,&s);
  sample_json(&b,&s,++seq,GetTickCount64());
  int published=publish(&b);
  if(!published&&!publication_failed)message(1,"Status publication failed; retrying while camera heartbeat expires");
  if(published&&publication_failed)message(0,"Status publication recovered");
  publication_failed=!published;
  /* A failed atomic replace must not slow the next fresh sample. Two 250ms
     backoffs exceeded Lua's 350ms heartbeat limit even for brief contention. */
  Sleep(50);
 }
 atomic_store_explicit(&native_phase,3,memory_order_release);
 common(&b,"LIMIT",seq,GetTickCount64());add(&b,",\"reason\":\"TWELVE_HOUR_MONITOR_LIMIT\"}\n");if(!publish(&b))message(1,"Limit status publication failed");return 0;
}
EXPORT void reframework_plugin_required_version(REFVersion*v){v->major=1;v->minor=0;v->patch=0;v->game_name="RE7";}
EXPORT _Bool reframework_plugin_initialize(const REFInit*p){
 if(initialized)return 1;
 if(!p||!p->functions)return 0;
 ref=p->functions;
 if(!setup_paths()||!setup_session()){message(1,"Wrong executable or cannot create Explore folder");return 0;}
 HMODULE self;if(!GetModuleHandleExW(5,(const WCHAR*)&marker,&self))return 0;
 initialized=1;HANDLE thread=CreateThread(0,0,monitor,0,0,0);
 if(!thread){initialized=0;message(1,"Monitor thread creation failed");return 0;}CloseHandle(thread);
 if(!ref->on_lua_created||!ref->on_lua_created(lua_created))message(1,"Lua callback unavailable; legacy file transport only");
 message(0,"v" EXP_VERSION " monitor started automatically. Lua camera starts OFF; F6 toggles.");return 1;
}
