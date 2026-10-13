/* Native observer: own-process reads and own log writes only. No game calls. */
#include "win_min.h"
#include "ref_min.h"
#include "observer.h"
#ifdef DIAG_HOST_TEST
#define EXPORT
#else
#define EXPORT __declspec(dllexport)
#endif
WIN_IMPORT void WINAPI Sleep(u32);
WIN_IMPORT void* WINAPI GetForegroundWindow(void);
WIN_IMPORT u32 WINAPI GetWindowThreadProcessId(void*,u32*);
WIN_IMPORT u32 WINAPI GetCurrentProcessId(void);
#ifndef DIAG_HOST_TEST
int _fltused=0; /* MSVC ABI floating-point marker; no CRT operations required. */
#endif
#define PATH_CAP 32768u
#define BUF_CAP 8192u
static const REFFunctions*ref;
static int initialized,busy,stop_requested,stage,callback_lock;
static u32 sessions;static int f8_down,f7_down;
static char marker;
static WCHAR exe_path[PATH_CAP],folder[PATH_CAP],status_path[PATH_CAP],log_path[PATH_CAP];
static u32 folder_len;
static char session[96];
static u8 hash_buffer[1024*1024],pe_header[65536];
static HANDLE logfile=INVALID_HANDLE;
static u64 bytes_written;
static const char*stages[]={"STILL","MOVE","LOOK","AIM","INVENTORY","EXAMINE","RELOAD","AFTER_RELOAD"};
typedef struct {char data[BUF_CAP];u32 n;int failed;} Buffer;
static void add(Buffer*b,const char*s){while(*s){if(b->n>=BUF_CAP){b->failed=1;return;}b->data[b->n++]=*s++;}}
static void number(Buffer*b,u64 n){char tmp[32];u32 k=0;do{tmp[k++]=(char)('0'+n%10);n/=10;}while(n);while(k){char c[2]={tmp[--k],0};add(b,c);}}
static void quote(Buffer*b,const char*s){add(b,"\"");add(b,s);add(b,"\"");} /* Only internal literals/numeric session identifiers. */
static void hex(Buffer*b,u64 n){char tmp[16];u32 k=0;add(b,"\"0x");do{tmp[k++]="0123456789ABCDEF"[n&15];n>>=4;}while(n);while(k){char c[2]={tmp[--k],0};add(b,c);}add(b,"\"");}
static void decimal(Buffer*b,float f){double d=f;if(d<0){add(b,"-");d=-d;}u64 scaled=(u64)(d*1000000.0+0.5);number(b,scaled/1000000);add(b,".");u64 rem=scaled%1000000;u32 div=100000;while(div){char c[2]={(char)('0'+rem/div),0};add(b,c);rem%=div;div/=10;}}
static void message(int error,const char*s){if(ref){if(error&&ref->log_error)ref->log_error("[RE7_OBSERVER] %s",s);else if(ref->log_info)ref->log_info("[RE7_OBSERVER] %s",s);}}
static int append(WCHAR*p,u32*n,const char*s){while(*s){if(*n+1>=PATH_CAP)return 0;p[(*n)++]=(WCHAR)(u8)*s++;}p[*n]=0;return 1;}
static int paths(void){u32 n=GetModuleFileNameW(0,exe_path,PATH_CAP);if(!n||n>=PATH_CAP)return 0;u32 start=n;while(start&&exe_path[start-1]!='\\'&&exe_path[start-1]!='/')start--;const char*want="re7.exe";if(n-start!=7)return 0;for(u32 i=0;i<7;i++){u16 c=exe_path[start+i];if(c>='A'&&c<='Z')c+=32;if(c!=(u16)want[i])return 0;}
 diag_copy(folder,exe_path,start*2);n=start;
 const char*dirs[]={"reframework","\\data","\\RE7_OTS_Observer"};
 for(u32 i=0;i<3;i++){if(!append(folder,&n,dirs[i]))return 0;if(!CreateDirectoryW(folder,0)&&GetLastError()!=183)return 0;}
 folder_len=n;diag_copy(status_path,folder,(n+1)*2);return append(status_path,&n,"\\status.json");
}
static int read_memory(void*ctx,u64 a,void*b,usize n){(void)ctx;usize got=0;return n&&a<=~(u64)0-n&&ReadProcessMemory(GetCurrentProcess(),(void*)(usize)a,b,n,&got)&&got==n;}
static int write_all(HANDLE h,const Buffer*b){if(b->failed)return 0;u32 done=0;while(done<b->n){u32 n=0;if(!WriteFile(h,b->data+done,b->n-done,&n,0)||!n||n>b->n-done)return 0;done+=n;}return 1;}
static int log_line(const Buffer*b){if(b->n>OBS_MAX_BYTES-bytes_written)return 0;if(!write_all(logfile,b))return 0;bytes_written+=b->n;return 1;}
static int status(const Buffer*b){HANDLE h=CreateFileW(status_path,GENERIC_WRITE,7,0,2,FILE_ATTRIBUTE_NORMAL,0);if(h==INVALID_HANDLE)return 0;int ok=write_all(h,b);if(!CloseHandle(h))ok=0;return ok;}
static void common(Buffer*b,const char*kind,const char*phase,u32 seq,u64 now,u64 start,u32 st){
 add(b,"{\"schema\":1,\"kind\":");quote(b,kind);add(b,",\"version\":\"" OBS_VERSION "\",\"session\":");quote(b,session);add(b,",\"phase\":");quote(b,phase);add(b,",\"seq\":");number(b,seq);add(b,",\"tick_ms\":");number(b,now);add(b,",\"elapsed_ms\":");number(b,now>=start?now-start:0);add(b,",\"stage\":");quote(b,stages[st<8?st:7]);
}
static void value(Buffer*b,const char*name,const ObsValue*v,int xyz){add(b,",\"");add(b,name);add(b,"\":{\"status\":");quote(b,obs_status_name(v->status));add(b,",\"fail_step\":");if(v->status==OBS_OK)add(b,"null");else number(b,v->fail_step);add(b,",\"generation\":");number(b,v->generation);
 if(xyz){add(b,",\"xyz\":");if(v->status==OBS_OK){add(b,"[");for(u32 i=0;i<3;i++){if(i)add(b,",");decimal(b,v->xyz[i]);}add(b,"]");}else add(b,"null");}
 else{add(b,",\"raw\":");if(v->status==OBS_OK||v->status==OBS_INVALID_VALUE)number(b,v->raw);else add(b,"null");}add(b,"}");
}
static int new_log(void){SYSTEMTIME t;GetLocalTime(&t);static Buffer name;diag_zero(&name,sizeof name);add(&name,"re7_observer_");number(&name,t.year);add(&name,"_");number(&name,t.month);add(&name,"_");number(&name,t.day);add(&name,"_");number(&name,t.hour);add(&name,"_");number(&name,t.minute);add(&name,"_");number(&name,t.second);add(&name,"_");number(&name,GetTickCount64());add(&name,"_");number(&name,sessions);
 if(name.n>=sizeof session)return 0;
 diag_copy(session,name.data,name.n);session[name.n]=0;u32 n=folder_len;diag_copy(log_path,folder,(n+1)*2);if(!append(log_path,&n,"\\")||!append(log_path,&n,session)||!append(log_path,&n,"_native.jsonl"))return 0;
 logfile=CreateFileW(log_path,GENERIC_WRITE,1,0,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,0);bytes_written=0;return logfile!=INVALID_HANDLE;
}
/* Exact executable identity gates fixed offsets. PE metadata alone is not a hash. */
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
  if(__atomic_load_n(&stop_requested,__ATOMIC_ACQUIRE)){error="CANCELLED";break;}
  u64 left=(u64)size.QuadPart-done;u32 want=left>sizeof hash_buffer?sizeof hash_buffer:(u32)left,got=0;
  if(!ReadFile(f,hash_buffer,want,&got,0)||!got||got>want){error="EXE_READ_FAILED";break;}
  diag_sha_update(&hash,hash_buffer,got);done+=got;
 }
 CloseHandle(f);if(error)return error;
 u8 digest[32];diag_sha_final(&hash,digest);for(u32 i=0;i<32;i++){digest_text[2*i]="0123456789abcdef"[digest[i]>>4];digest_text[2*i+1]="0123456789abcdef"[digest[i]&15];}digest_text[64]=0;
 return diag_equal(digest_text,OBS_EXE_HASH,64)?0:"UNSUPPORTED_EXE_HASH";
}
static u32 WINAPI observe_worker(void*unused){(void)unused;SetThreadPriority(GetCurrentThread(),-1);u64 start=GetTickCount64(),base=(u64)(usize)GetModuleHandleW(0);u32 count=0;const char*phase="ERROR",*reason="LOG_CREATE_FAILED";int log_ok=0;char hash[65]={0};ObsState state;diag_zero(&state,sizeof state);
 if(!new_log()){message(1,reason);goto finish;}
 static Buffer header;diag_zero(&header,sizeof header);common(&header,"header","VALIDATING",0,start,start,0);add(&header,",\"game_writes\":false,\"hooks\":false,\"semantic_validation\":\"NOT_PERFORMED\",\"atomic_snapshot\":false,\"camera_ct_equivalence\":\"NOT_TESTED\",\"sample_period_ms\":100,\"max_samples\":9000,\"max_duration_ms\":900000}\n");
 if(!log_line(&header)||!status(&header)){reason="LOG_OR_STATUS_WRITE_FAILED";goto finish;}
 reason=validate_build(base,hash);
 static Buffer identity;diag_zero(&identity,sizeof identity);add(&identity,"{\"kind\":\"build\",\"exe_sha256\":");quote(&identity,hash);add(&identity,",\"result\":");quote(&identity,reason?reason:"KNOWN_EXE_HASH_AND_PE");add(&identity,"}\n");
 if(!log_line(&identity)){reason="LOG_WRITE_FAILED";goto finish;}
 if(reason){phase=diag_equal(reason,"CANCELLED",10)?"STOPPED":"UNSUPPORTED";goto finish;}
 start=GetTickCount64();phase="STOPPED";reason="USER_STOP";
 while(!__atomic_load_n(&stop_requested,__ATOMIC_ACQUIRE)){
  u64 now=GetTickCount64();if(now-start>=OBS_MAX_MS||count>=OBS_MAX_SAMPLES||bytes_written>OBS_MAX_BYTES-BUF_CAP*2){phase="LIMIT";reason="RECORDING_LIMIT";break;}
  ObsSample sample;obs_sample(read_memory,0,base,1,&state,&sample);u32 st=(u32)__atomic_load_n(&stage,__ATOMIC_ACQUIRE);
  static Buffer b;diag_zero(&b,sizeof b);common(&b,"sample","RECORDING",++count,now,start,st);value(&b,"player",&sample.player,1);value(&b,"aim",&sample.aim,0);value(&b,"inventory",&sample.inventory,0);
  add(&b,",\"interact\":{\"status\":");quote(&b,obs_status_name(sample.interact.status));add(&b,",\"slot_hex\":");if(sample.interact.status==OBS_OK)hex(&b,sample.interact.raw);else add(&b,"null");add(&b,",\"ct_low16_nonzero\":");add(&b,sample.interact.status!=OBS_OK?"null":(sample.interact.raw&0xFFFF)?"true":"false");add(&b,",\"ct_low32_nonzero\":");add(&b,sample.interact.status!=OBS_OK?"null":(sample.interact.raw&0xFFFFFFFF)?"true":"false");add(&b,"}}\n");
  if(!log_line(&b)||!status(&b)||(count%10==0&&!FlushFileBuffers(logfile))){phase="ERROR";reason="LOG_OR_STATUS_WRITE_FAILED";break;}
  Sleep(OBS_PERIOD_MS);
 }
finish:
 {static Buffer end;diag_zero(&end,sizeof end);common(&end,"end",phase,count,GetTickCount64(),start,(u32)__atomic_load_n(&stage,__ATOMIC_ACQUIRE));add(&end,",\"reason\":");quote(&end,reason?reason:"UNKNOWN");add(&end,"}\n");
  if(logfile!=INVALID_HANDLE){log_ok=log_line(&end)&&FlushFileBuffers(logfile);if(!CloseHandle(logfile))log_ok=0;logfile=INVALID_HANDLE;}
  if(!log_ok){end.n=0;end.failed=0;common(&end,"end","ERROR",count,GetTickCount64(),start,0);add(&end,",\"reason\":\"LOG_WRITE_OR_CLOSE_FAILED\"}\n");message(1,"Recording log is incomplete or could not be created.");}
  if(!status(&end))message(1,"Status file write failed; UI may be stale.");
 }
 __atomic_store_n(&busy,0,__ATOMIC_RELEASE);return 0;
}
static void on_present(void){
 if(__atomic_exchange_n(&callback_lock,1,__ATOMIC_ACQUIRE))return;
 int k8=(GetAsyncKeyState(0x77)&0x8000)!=0,k7=(GetAsyncKeyState(0x76)&0x8000)!=0;u32 pid=0;void*w=GetForegroundWindow();if(w)GetWindowThreadProcessId(w,&pid);
 int focused=pid==GetCurrentProcessId();int active=__atomic_load_n(&busy,__ATOMIC_ACQUIRE);
 if(focused&&k8&&!f8_down){
  if(active)__atomic_store_n(&stop_requested,1,__ATOMIC_RELEASE);
  else if(sessions<OBS_MAX_SESSIONS){
   __atomic_store_n(&stop_requested,0,__ATOMIC_RELEASE);__atomic_store_n(&stage,0,__ATOMIC_RELEASE);__atomic_store_n(&busy,1,__ATOMIC_RELEASE);sessions++;
   HANDLE thread=CreateThread(0,0,observe_worker,0,0,0);if(thread)CloseHandle(thread);else{sessions--;__atomic_store_n(&busy,0,__ATOMIC_RELEASE);message(1,"Could not start observer thread; press F8 to retry.");}
  }else message(1,"Four session limit reached. Restart game for another recording.");
 }
 if(focused&&k7&&!f7_down&&active){int st=__atomic_load_n(&stage,__ATOMIC_RELAXED);if(st<7)__atomic_store_n(&stage,st+1,__ATOMIC_RELEASE);}
 f8_down=k8;f7_down=k7;__atomic_store_n(&callback_lock,0,__ATOMIC_RELEASE);
}
EXPORT void reframework_plugin_required_version(REFVersion*v){v->major=1;v->minor=0;v->patch=0;v->game_name="RE7";}
EXPORT _Bool reframework_plugin_initialize(const REFInit*p){
 if(initialized)return 1;
 if(!p||!p->functions||!p->functions->on_present)return 0;
 ref=p->functions;if(!paths()){message(1,"Cannot create RE7 observer folder or wrong executable name.");return 0;}
 HMODULE self;if(!GetModuleHandleExW(5,(const WCHAR*)&marker,&self))return 0;
 if(!ref->on_present(on_present))return 0;
 initialized=1;message(0,"v" OBS_VERSION " loaded. F8 starts/stops observation; F7 marks next test stage. No 3rd-person patch.");return 1;
}
