/* Native hook identity/order probe. Original instructions are always replayed;
 * no game data, camera, model, or firing vector is modified by this plugin. */
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#include <stdatomic.h>
#include "ref_min.h"
#include "observer.h"
#include "probe_capture.h"
#include "probe_lifecycle.h"
#include "probe_sites.h"
#include "original_scalar.h"
#include "MinHook.h"
#define PROBE_VERSION "0.1.0"
#define PROBE_LIMIT_MS 90000ULL
#define PROBE_LIMIT_BYTES (8u*1024u*1024u)
#define PATH_CAP 32768
static const REFFunctions *ref;
static WCHAR exe_path[PATH_CAP],root[PATH_CAP],folder[PATH_CAP],log_path[PATH_CAP];
static HANDLE logfile=INVALID_HANDLE_VALUE;
static uint64_t base,log_bytes,started,logged[5],omitted[5],camera_matches,read_failures;
static uint64_t candidates[64];static unsigned candidate_count,candidate_overflow;
static int log_failed,log_full,initialized,finalizing;
static _Atomic int phase,start_request,stop_request;
enum{VALIDATING,READY,INSTALLING,RUNNING,DONE,BLOCKED};
static ProbeLifecycle lifecycle;
static char reason[96]="VALIDATING";
static uint8_t hash_buffer[1024*1024],pe_buffer[65536];
static int fail(const char*s){snprintf(reason,sizeof reason,"%s",s);return 0;}
static void message(const char*s){if(ref&&ref->log_info)ref->log_info("[RE7_NATIVE_PROBE] %s",s);}
static int read_mem(void*ctx,u64 addr,void*out,usize n){
 (void)ctx;SIZE_T got=0;return n&&addr<=(u64)UINTPTR_MAX-n&&ReadProcessMemory(GetCurrentProcess(),(void*)(uintptr_t)addr,out,n,&got)&&got==n;
}
static uint32_t fbits(float f){uint32_t u;memcpy(&u,&f,4);return u;}
static int line(const char*fmt,...){
 char buf[4096];va_list args;va_start(args,fmt);int n=vsnprintf(buf,sizeof buf,fmt,args);va_end(args);
 if(n<0||(size_t)n>=sizeof buf){log_failed=1;return 0;}
 if(log_bytes+(unsigned)n>PROBE_LIMIT_BYTES-(finalizing?0:8192)){log_full=1;return 0;}
 DWORD got=0;if(logfile==INVALID_HANDLE_VALUE||!WriteFile(logfile,buf,(DWORD)n,&got,NULL)||got!=(DWORD)n){log_failed=1;return 0;}
 log_bytes+=got;return 1;
}
static int join(WCHAR*out,const WCHAR*prefix,const WCHAR*suffix){
 size_t a=wcslen(prefix),b=wcslen(suffix);if(a+b>=PATH_CAP)return 0;
 memcpy(out,prefix,a*sizeof(WCHAR));memcpy(out+a,suffix,(b+1)*sizeof(WCHAR));return 1;
}
static int setup(void){
 DWORD n=GetModuleFileNameW(NULL,exe_path,PATH_CAP);if(!n||n>=PATH_CAP)return 0;
 WCHAR*slash=wcsrchr(exe_path,L'\\');if(!slash)slash=wcsrchr(exe_path,L'/');
 if(!slash||_wcsicmp(slash+1,L"re7.exe"))return 0;
 size_t k=(size_t)(slash-exe_path);memcpy(root,exe_path,k*sizeof(WCHAR));root[k]=0;
 const WCHAR*dirs[]={L"\\reframework",L"\\reframework\\data",L"\\reframework\\data\\RE7_OTS_NativeProbe"};
 for(unsigned i=0;i<3;i++){if(!join(folder,root,dirs[i]))return 0;if(!CreateDirectoryW(folder,NULL)&&GetLastError()!=ERROR_ALREADY_EXISTS)return 0;}
 WCHAR name[100];swprintf(name,100,L"\\probe_%llu_%lu.jsonl",(unsigned long long)GetTickCount64(),(unsigned long)GetCurrentProcessId());
 if(!join(log_path,folder,name))return 0;
 logfile=CreateFileW(log_path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
 return logfile!=INVALID_HANDLE_VALUE;
}
static int conflict(void){
 static WCHAR path[PATH_CAP];
 if(GetModuleHandleW(L"RE7_OTS_Explore.dll"))return fail("EXPLORE_DLL_LOADED");
 if(!join(path,root,L"\\reframework\\autorun\\re7_ots_explore.lua"))return fail("PATH_TOO_LONG");
 if(GetFileAttributesW(path)!=INVALID_FILE_ATTRIBUTES)return fail("EXPLORE_LUA_PRESENT");
 DWORD error=GetLastError();if(error!=ERROR_FILE_NOT_FOUND&&error!=ERROR_PATH_NOT_FOUND)return fail("EXPLORE_CHECK_FAILED");
 return 1;
}
static int known_build(void){
 if(!read_mem(NULL,base,pe_buffer,64))return fail("PE_READ_FAILED");
 uint32_t nt=diag_u32(pe_buffer+60);if(nt<64||nt>sizeof(pe_buffer)-24)return fail("PE_HEADER_INVALID");
 if(!read_mem(NULL,base+nt,pe_buffer+nt,24))return fail("PE_READ_FAILED");
 uint64_t n=(uint64_t)nt+24+diag_u16(pe_buffer+nt+20)+(uint64_t)diag_u16(pe_buffer+nt+6)*40;
 if(n>sizeof(pe_buffer)||!read_mem(NULL,base,pe_buffer,(size_t)n))return fail("PE_READ_FAILED");
 PE pe;if(!diag_parse_pe(pe_buffer,(size_t)n,0,&pe)||pe.timestamp!=OBS_TIMESTAMP||pe.image_size!=OBS_IMAGE_SIZE)return fail("UNSUPPORTED_PE");
 HANDLE f=CreateFileW(exe_path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,0,NULL);
 if(f==INVALID_HANDLE_VALUE)return fail("EXE_OPEN_FAILED");
 LARGE_INTEGER size;int ok=GetFileSizeEx(f,&size)&&size.QuadPart>0&&size.QuadPart<=DIAG_MAX_IMAGE;
 Sha256 h;diag_sha_init(&h);uint64_t done=0;
 while(ok&&done<(uint64_t)size.QuadPart){DWORD got=0;uint64_t left=(uint64_t)size.QuadPart-done;
  DWORD want=left>sizeof(hash_buffer)?sizeof(hash_buffer):(DWORD)left;
  if(!ReadFile(f,hash_buffer,want,&got,NULL)||!got||got>want){ok=0;break;}diag_sha_update(&h,hash_buffer,got);done+=got;
 }
 CloseHandle(f);if(!ok)return fail("EXE_HASH_READ_FAILED");
 uint8_t digest[32];char hex[65];diag_sha_final(&h,digest);for(unsigned i=0;i<32;i++)snprintf(hex+i*2,3,"%02x",digest[i]);
 return strcmp(hex,OBS_EXE_HASH)==0?1:fail("UNSUPPORTED_EXE_HASH");
}
static int verify_sites(void*unused){
 (void)unused;if(!conflict())return 0;
 for(unsigned i=0;i<5;i++){
  const ProbeSite*s=&probe_sites[i];uint8_t bytes[32];MEMORY_BASIC_INFORMATION info;
  uintptr_t target=(uintptr_t)(base+s->rva);
  if(!VirtualQuery((void*)target,&info,sizeof info)||info.State!=MEM_COMMIT||
     (info.Protect&(PAGE_NOACCESS|PAGE_GUARD))||!(info.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY))||
     target-(uintptr_t)info.BaseAddress>info.RegionSize||s->length>info.RegionSize-(target-(uintptr_t)info.BaseAddress)||
     !read_mem(NULL,target,bytes,s->length)||memcmp(bytes,s->expected,s->length)){
   snprintf(reason,sizeof reason,"HOOK_BYTES_OR_PROTECTION_%u",i);return 0;
  }
 }
 return 1;
}
static int create_site(void*unused,unsigned i){
 (void)unused;MH_STATUS s=MH_CreateHook((void*)(uintptr_t)(base+probe_sites[i].rva),probe_detours[i],&probe_trampolines[i]);
 if(s!=MH_OK){snprintf(reason,sizeof reason,"CREATE_%u_STATUS_%d",i,s);return 0;}return 1;
}
static int enable_all(void*unused){
 (void)unused;
 for(unsigned i=0;i<5;i++){MH_STATUS s=MH_QueueEnableHook((void*)(uintptr_t)(base+probe_sites[i].rva));if(s!=MH_OK){snprintf(reason,sizeof reason,"QUEUE_%u_STATUS_%d",i,s);return 0;}}
 MH_STATUS s=MH_ApplyQueued();if(s!=MH_OK){snprintf(reason,sizeof reason,"ENABLE_STATUS_%d",s);return 0;}return 1;
}
static int disable_all(void*unused){
 (void)unused;return MH_DisableHook(MH_ALL_HOOKS)==MH_OK;
 /* Never remove/uninitialize/free here, even if disable succeeded. */
}
static int foreground(void){DWORD pid=0;HWND w=GetForegroundWindow();if(w)GetWindowThreadProcessId(w,&pid);return pid==GetCurrentProcessId();}
static void present(void){
 atomic_fetch_add_explicit(&probe_frame,1,memory_order_relaxed);
 int front=foreground(),p=atomic_load_explicit(&phase,memory_order_acquire);
 static int down;int pressed=front&&(GetAsyncKeyState(VK_F6)&0x8000);
 if(pressed&&!down){if(p==READY)atomic_store(&start_request,1);else if(p==RUNNING){atomic_fetch_or(&probe_enabled,4);atomic_store(&stop_request,1);}}
 down=pressed;
 if(front)atomic_fetch_or_explicit(&probe_enabled,2,memory_order_release);
 else atomic_fetch_and_explicit(&probe_enabled,~2u,memory_order_release);
}
static int candidate(uint64_t addr){for(unsigned i=0;i<candidate_count;i++)if(candidates[i]==addr)return 1;return 0;}
static void event(unsigned site,const ProbeRecord*e,int emit){
 if(site==0&&e->rdi>=0xA0){uint64_t c=e->rdi-0xA0;if(!candidate(c)){if(candidate_count<64)candidates[candidate_count++]=c;else candidate_overflow++;}}
 int match=site==1&&candidate(e->rbx);if(match)camera_matches++;
 if(!emit){omitted[site]++;return;}
 logged[site]++;
 line("{\"type\":\"hook\",\"site\":%u,\"seq\":%llu,\"frame\":%llu,\"rax\":\"0x%llx\",\"rbx\":\"0x%llx\",\"rcx\":\"0x%llx\",\"rdx\":\"0x%llx\",\"rdi\":\"0x%llx\",\"rsi\":\"0x%llx\",\"rbp\":\"0x%llx\",\"xmm_bits\":[%u,%u,%u,%u],\"capture_address_overlap\":%s}\n",
 site,(unsigned long long)e->sequence,(unsigned long long)e->frame,(unsigned long long)e->rax,(unsigned long long)e->rbx,(unsigned long long)e->rcx,(unsigned long long)e->rdx,(unsigned long long)e->rdi,(unsigned long long)e->rsi,(unsigned long long)e->rbp,fbits(e->xmm[0]),fbits(e->xmm[1]),fbits(e->xmm[2]),fbits(e->xmm[3]),match?"true":"false");
 /* Explicitly asynchronous preview, never evidence of same-hook memory state. */
 if(site==1&&match){float v[16];if(!read_mem(NULL,e->rbx,v,sizeof v)){read_failures++;return;}
  if(isfinite(v[0])&&isfinite(v[2])&&fabsf(v[0])<=1.1f&&fabsf(v[2])<=1.1f&&isfinite(e->xmm[3])){
   Re7OriginalParams p;re7_original_defaults(&p);p.pitchmul=re7_original_pitch_multiplier(e->xmm[3]);
   float xyz[3];for(unsigned i=0;i<3;i++)xyz[i]=re7_original_ots_axis(i,e->xmm[i],v[2],v[0],e->xmm[3],&p);
   line("{\"type\":\"asynchronous_formula_preview\",\"seq\":%llu,\"xyz_bits\":[%u,%u,%u],\"game_write\":false}\n",(unsigned long long)e->sequence,fbits(xyz[0]),fbits(xyz[1]),fbits(xyz[2]));
  }
 }
 if(site==4){float d;if(read_mem(NULL,e->rcx+0x18,&d,4))line("{\"type\":\"asynchronous_dof_read\",\"seq\":%llu,\"distance_bits\":%u}\n",(unsigned long long)e->sequence,fbits(d));else read_failures++;}
}
static void drain(unsigned allowance){
 ProbeRecord events[PROBE_CAPACITY];
 for(unsigned i=0;i<5;i++){size_t n=probe_drain(i,events,PROBE_CAPACITY);for(size_t j=0;j<n;j++)event(i,&events[j],j<allowance);}
 /* Per-site queues are drained independently. seq, not file line order,
  * establishes cross-site order. Missing records prevent completeness claims. */
}
static void summary(const char*status){
 finalizing=1;
 line("{\"type\":\"summary\",\"status\":\"%s\",\"reason\":\"%s\",\"camera_address_overlaps\":%llu,\"candidate_count\":%u,\"candidate_overflow\":%u,\"async_read_failures\":%llu,\"frame\":%llu,\"log_failed\":%s,\"log_limit\":%s,\"trampolines_retained\":%s}\n",status,reason,(unsigned long long)camera_matches,candidate_count,candidate_overflow,(unsigned long long)read_failures,(unsigned long long)atomic_load(&probe_frame),log_failed?"true":"false",log_full?"true":"false",lifecycle.retain_trampolines?"true":"false");
 for(unsigned i=0;i<5;i++)line("{\"type\":\"site_summary\",\"site\":%u,\"name\":\"%s\",\"calls\":%llu,\"dropped\":%llu,\"logged\":%llu,\"sampled_out\":%llu}\n",i,probe_sites[i].name,(unsigned long long)atomic_load(&probe_queues[i].calls),(unsigned long long)atomic_load(&probe_queues[i].drops),(unsigned long long)logged[i],(unsigned long long)omitted[i]);
 FlushFileBuffers(logfile);finalizing=0;
}
static DWORD WINAPI worker(void*unused){
 (void)unused;SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_BELOW_NORMAL);
 if(!setup()){message("Cannot open capture log or wrong executable; no hooks installed.");atomic_store(&phase,BLOCKED);return 0;}
 base=(uint64_t)(uintptr_t)GetModuleHandleW(NULL);
 line("{\"type\":\"header\",\"schema\":1,\"version\":\"" PROBE_VERSION "\",\"mode\":\"PASS_THROUGH_HOOK_PROBE\",\"source_commit\":\"" PROBE_SOURCE_COMMIT "\",\"game_data_writes\":false,\"code_patches\":true,\"limit_ms\":90000,\"limit_bytes\":8388608,\"build_sha256\":\"" OBS_EXE_HASH "\"}\n");
 if(log_failed||!known_build()||!verify_sites(NULL)){atomic_store(&phase,BLOCKED);summary("BLOCKED");message(reason);goto done;}
 if(MH_Initialize()!=MH_OK){fail("MINHOOK_INIT_FAILED");atomic_store(&phase,BLOCKED);summary("BLOCKED");goto done;}
 fail("READY_F6");atomic_store_explicit(&phase,READY,memory_order_release);summary("READY");message("Ready. F6 starts one 90-second original-hook capture. Camera stays first person.");
 while(!atomic_load(&start_request))Sleep(25);
 atomic_store(&phase,INSTALLING);
 ProbeOps ops={verify_sites,create_site,enable_all,disable_all};
 if(!probe_install(&lifecycle,&ops,NULL)){atomic_store(&probe_enabled,0);atomic_store(&phase,BLOCKED);summary("HOOK_INSTALL_FAILED");message(reason);goto done;}
 started=GetTickCount64();atomic_fetch_or(&probe_enabled,1);fail("CAPTURING");atomic_store_explicit(&phase,RUNNING,memory_order_release);message("Capturing original hooks. F6 stops; automatic stop after 90 seconds.");
 ObsState os={0};uint64_t next_sample=0;
 while(GetTickCount64()-started<PROBE_LIMIT_MS&&!atomic_load(&stop_request)&&!log_failed&&!log_full){
  uint64_t now=GetTickCount64();drain(1);
  if(now>=next_sample){ObsSample sample;obs_sample(read_mem,NULL,base,1,&os,&sample);
   line("{\"type\":\"state\",\"elapsed_ms\":%llu,\"frame\":%llu,\"player_status\":\"%s\",\"player_bits\":[%u,%u,%u],\"aim_status\":\"%s\",\"aim\":%llu,\"inventory_status\":\"%s\",\"inventory\":%llu,\"interact_status\":\"%s\",\"interact\":\"0x%llx\"}\n",(unsigned long long)(now-started),(unsigned long long)atomic_load(&probe_frame),obs_status_name(sample.player.status),fbits(sample.player.xyz[0]),fbits(sample.player.xyz[1]),fbits(sample.player.xyz[2]),obs_status_name(sample.aim.status),(unsigned long long)sample.aim.raw,obs_status_name(sample.inventory.status),(unsigned long long)sample.inventory.raw,obs_status_name(sample.interact.status),(unsigned long long)sample.interact.raw);
   next_sample=now+250;
  }Sleep(20);
 }
 atomic_fetch_or(&probe_enabled,4);atomic_store(&phase,DONE);drain(1);
 fail(log_failed?"LOG_WRITE_FAILED":log_full?"LOG_SIZE_LIMIT":atomic_load(&stop_request)?"USER_STOPPED":"TIME_LIMIT");
 summary("CAPTURE_FINISHED_REVIEW_REQUIRED");if(log_failed)message("Log write failed. Check free space and reframework.log.");else message("Capture finished. Send reframework/data/RE7_OTS_NativeProbe/probe_*.jsonl. Restart before another capture.");
 done:CloseHandle(logfile);logfile=INVALID_HANDLE_VALUE;return 0;
}
__declspec(dllexport) void reframework_plugin_required_version(REFVersion*v){v->major=1;v->minor=0;v->patch=0;v->game_name="RE7";}
__declspec(dllexport) _Bool reframework_plugin_initialize(const REFInit*p){
 if(initialized)return 1;
 if(!p||!p->functions||!p->functions->on_present)return 0;
 ref=p->functions;
 HMODULE self;if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,(LPCWSTR)(uintptr_t)&reframework_plugin_initialize,&self))return 0;
 if(!ref->on_present(present))return 0;
 HANDLE thread=CreateThread(NULL,0,worker,NULL,0,NULL);if(!thread)return 0;
 CloseHandle(thread);initialized=1;message("Native probe v" PROBE_VERSION " validating build; original third-person control is not enabled.");return 1;
}
