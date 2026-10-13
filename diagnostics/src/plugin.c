/* RE7 OTS RT read-only diagnostic. No CT execution, game writes or hooks. */
#include "win_min.h"
#include "ref_min.h"
#include "manifest.h"
#define VERSION "0.1.0"
#define CHUNK (1024u*1024u)
#define PATH_CAP 32768u
#define HEADER_CAP 65536u
#define LOG_CAP 4096u
#ifdef DIAG_HOST_TEST
#define EXPORT
#else
#define EXPORT __declspec(dllexport)
#endif
static const REFFunctions* g_ref;
static int g_ref_major,g_ref_minor,g_ref_patch;
static u32 g_frames,g_sequence;
static int g_busy,g_once,g_initialized,g_keydown;
static char g_module_marker;
static WCHAR g_exe[PATH_CAP],g_path[PATH_CAP],g_status_path[PATH_CAP];
static char g_report_name[96];
static HANDLE g_log=INVALID_HANDLE,g_file=INVALID_HANDLE;
static u32 g_dirlen,g_logused;static int g_logfailed;
static char g_logbuf[LOG_CAP];
static u8 g_header[HEADER_CAP];
static u8* g_buffer;static usize g_base;
static PE g_disk_pe,g_mem_pe;
static int g_disk_valid,g_mem_valid,g_disk_complete,g_memory_complete;
static Pattern g_patterns[PATCH_COUNT],g_expected[PATCH_COUNT];
static Matches g_disk_matches[PATCH_COUNT],g_live_matches[PATCH_COUNT];
static u64 g_disk_size,g_scanned_disk,g_scanned_memory,g_skipped_memory,g_failed_memory;
static usize slen(const char*s){usize n=0;while(s[n])n++;return n;}
static void flush_log(void){u32 done=0;while(done<g_logused&&!g_logfailed){u32 n=0;if(!WriteFile(g_log,g_logbuf+done,g_logused-done,&n,0)||!n){g_logfailed=1;break;}done+=n;}g_logused=0;}
static void text(const char*s){while(*s){if(g_logused==LOG_CAP)flush_log();if(g_logfailed)return;g_logbuf[g_logused++]=*s++;}}
static void number(u64 n){char b[32];u32 k=0;do{b[k++]=(char)('0'+n%10);n/=10;}while(n);while(k){char c[2]={b[--k],0};text(c);}}
static void hx(u64 n){char b[19];u32 k=0;text("0x");do{b[k++]="0123456789ABCDEF"[n&15];n>>=4;}while(n);while(k){char c[2]={b[--k],0};text(c);}}
static void hexbytes(const u8*p,u32 n){for(u32 i=0;i<n;i++){char c[4]={"0123456789ABCDEF"[p[i]>>4],"0123456789ABCDEF"[p[i]&15],' ',0};if(i+1==n)c[2]=0;text(c);}}
static void kv(const char*k,const char*v){text(k);text("=");text(v);text("\n");}
static void numkv(const char*k,u64 v){text(k);text("=");number(v);text("\n");}
static void hexkv(const char*k,u64 v){text(k);text("=");hx(v);text("\n");}
static void checkpoint(void){flush_log();if(!g_logfailed&&!FlushFileBuffers(g_log))g_logfailed=1;}
static void ref_info(const char*s){if(g_ref&&g_ref->log_info)g_ref->log_info("[RE7_OTS_DIAG] %s",s);}
static void ref_error(const char*s){OutputDebugStringA(s);if(g_ref&&g_ref->log_error)g_ref->log_error("[RE7_OTS_DIAG] %s",s);}
static int append_ascii(WCHAR*b,u32*used,const char*s){usize n=slen(s);if((u64)*used+n+1>PATH_CAP)return 0;while(*s)b[(*used)++]=(WCHAR)(u8)*s++;b[*used]=0;return 1;}
static int path_number(WCHAR*b,u32*used,u32 n,u32 width){char a[16];if(width>10)return 0;for(u32 i=0;i<width;i++){a[width-i-1]=(char)('0'+n%10);n/=10;}a[width]=0;return append_ascii(b,used,a);}
static int target_path(void){u32 n=GetModuleFileNameW(0,g_exe,PATH_CAP);if(!n||n>=PATH_CAP)return 0;u32 start=n;while(start&&g_exe[start-1]!='\\'&&g_exe[start-1]!='/')start--;const char*want="re7.exe";if(n-start!=7)return 0;for(u32 i=0;i<7;i++){u16 c=g_exe[start+i];if(c>='A'&&c<='Z')c+=32;if(c!=(u16)want[i])return 0;}g_dirlen=start;return 1;}
static int make_log(void){u32 n=g_dirlen;diag_copy(g_path,g_exe,(usize)n*sizeof(WCHAR));g_path[n]=0;
 const char* dirs[]={"reframework","\\data","\\RE7_OTS_Diagnostics"};
 for(u32 i=0;i<3;i++){if(!append_ascii(g_path,&n,dirs[i]))return 0;if(!CreateDirectoryW(g_path,0)&&GetLastError()!=183)return 0;}
 u32 folder_end=n;diag_copy(g_status_path,g_path,((usize)n+1)*sizeof(WCHAR));u32 sn=n;if(!append_ascii(g_status_path,&sn,"\\status.json"))return 0;
 SYSTEMTIME st;GetLocalTime(&st);
 if(!append_ascii(g_path,&n,"\\re7_ots_diagnostic_")||!path_number(g_path,&n,st.year,4)||!path_number(g_path,&n,st.month,2)||!path_number(g_path,&n,st.day,2)||!append_ascii(g_path,&n,"_")||!path_number(g_path,&n,st.hour,2)||!path_number(g_path,&n,st.minute,2)||!path_number(g_path,&n,st.second,2)||!append_ascii(g_path,&n,"_")||!path_number(g_path,&n,st.millisecond,3)||!append_ascii(g_path,&n,"_")||!path_number(g_path,&n,g_sequence++,3)||!append_ascii(g_path,&n,".txt"))return 0;
 if(n-folder_end>=sizeof g_report_name)return 0;for(u32 i=folder_end+1;i<n;i++)g_report_name[i-folder_end-1]=(char)g_path[i];g_report_name[n-folder_end-1]=0;
 g_log=CreateFileW(g_path,GENERIC_WRITE,1,0,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,0);g_logfailed=0;g_logused=0;return g_log!=INVALID_HANDLE;
}
/* Status is local UI metadata, separate from the full report. Readers tolerate
   a partially written JSON file and retry; no game state is changed. */
static void write_status(const char*phase){
 char b[256];u32 used=0;const char*parts[]={"{\"phase\":\"",phase,"\",\"report\":\"",g_report_name,"\"}\n"};
 for(u32 i=0;i<5;i++){const char*t=parts[i];while(*t){if(used>=sizeof b)return;b[used++]=*t++;}}
 HANDLE h=CreateFileW(g_status_path,GENERIC_WRITE,7,0,2,FILE_ATTRIBUTE_NORMAL,0);if(h==INVALID_HANDLE)return;
 u32 done=0;while(done<used){u32 n=0;if(!WriteFile(h,b+done,used-done,&n,0)||!n)break;done+=n;}
 FlushFileBuffers(h);CloseHandle(h);
}
static int file_read(u64 offset,void*b,u32 n){if(g_file==INVALID_HANDLE||offset>g_disk_size||n>g_disk_size-offset)return 0;LARGE_INTEGER p;p.QuadPart=(i64)offset;if(!SetFilePointerEx(g_file,p,0,0))return 0;u32 done=0;while(done<n){u32 got=0;if(!ReadFile(g_file,(u8*)b+done,n-done,&got,0)||!got)return 0;done+=got;}return 1;}
static int mem_read(usize a,void*b,usize n){usize got=0;return n&&a<=~(usize)0-n&&ReadProcessMemory(GetCurrentProcess(),(void*)a,b,n,&got)&&got==n;}
/* Read only required PE header bytes: do not cross a no-access alignment gap. */
static int read_memory_pe(void){
 if(!mem_read(g_base,g_header,64)||g_header[0]!='M'||g_header[1]!='Z')return 0;
 u32 nt=diag_u32(g_header+60);
 if(nt<64||nt>HEADER_CAP-24||!mem_read(g_base+nt,g_header+nt,24))return 0;
 u32 count=diag_u16(g_header+nt+6),optsz=diag_u16(g_header+nt+20);
 u64 need=(u64)nt+24+optsz+(u64)count*40;
 if(need>HEADER_CAP||!count||count>DIAG_MAX_SECTIONS||optsz<112)return 0;
 if(!mem_read(g_base,g_header,(usize)need))return 0;
 return diag_parse_pe(g_header,(usize)need,0,&g_mem_pe);
}
static int is_readable(u32 state,u32 protect){if(state!=MEM_COMMIT||(protect&0x100)||(protect&0xff)==1)return 0;u32 p=protect&0xff;return p==2||p==4||p==8||p==0x20||p==0x40||p==0x80;}
static int is_executable(u32 protect){u32 p=protect&0xff;return p==0x10||p==0x20||p==0x40||p==0x80;}
static void scan_window(u32 rva,u32 n,u32 tail,Matches*results){for(u32 i=0;i<PATCH_COUNT;i++)diag_scan(g_buffer,n,rva,tail,&g_patterns[i],&results[i]);}
static u32 retain_tail(u32 n){u32 k=n<DIAG_MAX_PATTERN-1?n:DIAG_MAX_PATTERN-1;/* Forward copy is safe: destination is below source. */diag_copy(g_buffer,g_buffer+n-k,k);return k;}
static void disk_scan(void){g_disk_complete=1;g_scanned_disk=0;diag_zero(g_disk_matches,sizeof g_disk_matches);
 for(u32 i=0;i<g_disk_pe.count;i++){const Section*s=&g_disk_pe.sections[i];u32 done=0,tail=0;text("disk_section=");text(s->name);text(" rva=");hx(s->va);text(" raw_bytes=");number(s->raw_size);text("\n");checkpoint();
  while(done<s->raw_size){u32 n=s->raw_size-done;if(n>CHUNK)n=CHUNK;if(!file_read((u64)s->raw+done,g_buffer+tail,n)){g_disk_complete=0;kv("disk_read_error",s->name);break;}scan_window(s->va+done-tail,n+tail,tail,g_disk_matches);g_scanned_disk+=n;tail=retain_tail(n+tail);done+=n;}
 }
}
static void memory_scan(void){u64 pos=0;u32 tail=0;g_memory_complete=1;g_scanned_memory=0;g_skipped_memory=0;g_failed_memory=0;diag_zero(g_live_matches,sizeof g_live_matches);
 while(pos<g_mem_pe.image_size){MEMORY_BASIC_INFORMATION mbi;usize q=VirtualQuery((void*)(g_base+(usize)pos),&mbi,sizeof mbi);if(q!=sizeof mbi||!mbi.RegionSize){g_memory_complete=0;g_failed_memory+=g_mem_pe.image_size-pos;break;}
  usize rb=(usize)mbi.BaseAddress;if(rb>g_base+(usize)pos||mbi.RegionSize>~(usize)0-rb){g_memory_complete=0;break;}u64 end=(u64)(rb-g_base)+mbi.RegionSize;if(rb<g_base)end=mbi.RegionSize-(g_base-rb);if(end>g_mem_pe.image_size)end=g_mem_pe.image_size;if(end<=pos){g_memory_complete=0;break;}
  if(!is_readable(mbi.State,mbi.Protect)){g_skipped_memory+=end-pos;tail=0;pos=end;continue;}
  while(pos<end){u32 n=(u32)(end-pos);if(n>CHUNK)n=CHUNK;if(mem_read(g_base+(usize)pos,g_buffer+tail,n)){scan_window((u32)pos-tail,n+tail,tail,g_live_matches);g_scanned_memory+=n;tail=retain_tail(n+tail);}else{
    /* Re-read individual pages after a bulk race/failure; never zero-fill holes. */
    g_memory_complete=0;u32 done=0;while(done<n){u32 k=4096u-(u32)((g_base+(usize)pos+done)&4095u);if(k>n-done)k=n-done;
     if(mem_read(g_base+(usize)pos+done,g_buffer+tail,k)){scan_window((u32)pos+done-tail,k+tail,tail,g_live_matches);g_scanned_memory+=k;tail=retain_tail(k+tail);}else{g_failed_memory+=k;tail=0;}done+=k;}
   }pos+=n;
  }
 }
}
static void log_matches(const char*prefix,const Matches*m){text(prefix);text("_match_count=");number(m->count);text("\n");text(prefix);text("_candidate_rvas=");u32 n=m->count<DIAG_MAX_MATCHES?(u32)m->count:DIAG_MAX_MATCHES;for(u32 i=0;i<n;i++){if(i)text(",");hx(m->rva[i]);}if(m->count>DIAG_MAX_MATCHES)text(",...truncated");text("\n");}
static int compare_at(int disk,u32 rva,const Pattern*p,u8*out){if(disk){u64 raw;if(!diag_rva_raw(&g_disk_pe,rva,p->n,&raw)||!file_read(raw,out,p->n))return -1;}else if((u64)rva+p->n>g_mem_pe.image_size||!mem_read(g_base+rva,out,p->n))return -1;return diag_equal(out,p->bytes,p->n);}
static const char*cmpname(int r){return r<0?"UNREADABLE_OR_UNMAPPED":r?"MATCH_CT_RESTORE_BYTES":"DIFFERENT_FROM_CT_RESTORE_BYTES";}
static u32 report_sites(void){u32 matched=0;
 for(u32 i=0;i<PATCH_COUNT;i++){const PatchDef*d=&PATCHES[i];const Pattern*e=&g_expected[i];const Matches*dm=&g_disk_matches[i],*mm=&g_live_matches[i];u32 dr=0,mr=0;int dc=-1,mc=-1,ld=-1,dl=-1;u8 db[DIAG_MAX_PATTERN],lb[DIAG_MAX_PATTERN];
  text("\n[PATCH ");number(i+1);text("]\n");kv("name",d->name);kv("group",d->group);numkv("ct_aob_line",d->ct_line);kv("pattern",d->pattern);text("site_delta=");if(d->delta<0){text("-");number((u32)-d->delta);}else number((u32)d->delta);text("\n");kv("expected_from_ct_disable",d->expected);
  log_matches("disk",dm);log_matches("memory",mm);
  int drvalid=g_disk_valid&&dm->count==1&&diag_patch_rva(dm->rva[0],d->delta,g_disk_pe.image_size,e->n,&dr);
  int mrvalid=g_mem_valid&&mm->count==1&&diag_patch_rva(mm->rva[0],d->delta,g_mem_pe.image_size,e->n,&mr);
  if(drvalid){hexkv("disk_patch_rva",dr);dc=compare_at(1,dr,e,db);ld=compare_at(0,dr,e,lb);kv("disk_original",cmpname(dc));kv("live_at_disk_candidate",cmpname(ld));if(dc>=0){text("disk_bytes=");hexbytes(db,e->n);text("\n");}if(ld>=0){text("live_at_disk_bytes=");hexbytes(lb,e->n);text("\n");}if(dc>=0&&ld>=0){dl=diag_equal(db,lb,e->n);kv("live_equals_disk_at_candidate",dl?"YES":"NO; cause not determined");}}
  if(mrvalid){hexkv("memory_patch_rva",mr);mc=compare_at(0,mr,e,lb);kv("memory_original",cmpname(mc));if(mc>=0){text("memory_bytes=");hexbytes(lb,e->n);text("\n");}}
  const char*assessment="REVIEW_REQUIRED";
  if(dm->count>1||mm->count>1)assessment="AMBIGUOUS_PATTERN";
  else if(drvalid&&mrvalid&&dr!=mr)assessment="DISK_MEMORY_LOCATION_DIFFERENCE";
  else if(drvalid&&mrvalid&&dc==1&&mc==1&&ld==1&&dr==mr){assessment=(g_disk_complete&&g_memory_complete)?"BYTE_MATCH_ONLY":"BYTE_MATCH_ONLY_INCOMPLETE_SCAN";matched++;}
  else if(drvalid&&dc==1&&ld==0)assessment="LIVE_BYTES_CHANGED_AT_DISK_CANDIDATE";
  else if(!dm->count&&!mm->count)assessment="PATTERN_NOT_FOUND_IN_SCANNED_SCOPES";
  else if(!g_disk_valid&&mrvalid&&mc==1)assessment="LIVE_BYTE_MATCH_DISK_UNAVAILABLE";
  else if(!dm->count&&mrvalid&&mc==1)assessment="MEMORY_ONLY_BYTE_MATCH";
  else if(dc==0||mc==0)assessment="FULL_PATCH_BYTES_MISMATCH";
  kv("assessment",assessment);if((i&7u)==7u)checkpoint();
 }
 return matched;
}
static void fixed_checks(void){text("\n[FIXED_RVAS]\n");kv("warning","Accessibility only. No proof of engine field identity or valid control flow.");
 for(u32 i=0;i<FIXED_COUNT;i++){const FixedDef*f=&FIXED[i];text("rva=");hx(f->rva);text(" kind=");text(f->kind);text(" ct_line=");number(f->ct_line);u32 n=f->kind[0]=='b'?1:8;
  if((u64)f->rva+n>g_mem_pe.image_size){text(" status=OUTSIDE_IMAGE\n");continue;}
  MEMORY_BASIC_INFORMATION mbi;u8 b[8];if(!mem_read(g_base+f->rva,b,n)){text(" status=UNREADABLE\n");continue;}
  text(" status=READABLE");if(VirtualQuery((void*)(g_base+f->rva),&mbi,sizeof mbi)==sizeof mbi){text(" executable=");text(is_executable(mbi.Protect)?"YES":"NO");}
  if(n==8){u64 v=diag_u64(b);text(" slot_all_zero=");text(v?"NO":"YES");/* Do not record addresses or dereference alleged objects. */}
  text(" semantic_validation=NOT_PERFORMED\n");
 }
}
static void file_fingerprint(void){Sha256 sh;u8 digest[32];u64 done=0;diag_sha_init(&sh);while(done<g_disk_size){u32 n=(u32)((g_disk_size-done)>CHUNK?CHUNK:(g_disk_size-done));if(!file_read(done,g_buffer,n)){kv("exe_sha256","UNAVAILABLE_READ_ERROR");return;}diag_sha_update(&sh,g_buffer,n);done+=n;}diag_sha_final(&sh,digest);text("exe_sha256=");for(u32 i=0;i<32;i++){char c[3]={"0123456789abcdef"[digest[i]>>4],"0123456789abcdef"[digest[i]&15],0};text(c);}text("\n");}
static u32 WINAPI worker(void*unused){(void)unused;SetThreadPriority(GetCurrentThread(),-1);u64 started=GetTickCount64();int completed=0;g_file=INVALID_HANDLE;g_buffer=0;g_disk_valid=0;g_mem_valid=0;g_disk_complete=0;g_memory_complete=0;g_disk_size=0;g_scanned_disk=0;g_scanned_memory=0;g_skipped_memory=0;g_failed_memory=0;diag_zero(g_disk_matches,sizeof g_disk_matches);diag_zero(g_live_matches,sizeof g_live_matches);
 if(!make_log()){ref_error("Could not create diagnostic log. Check game-folder write permission.");goto cleanup;}
 write_status("STARTING");
 text("RE7 OTS READ-ONLY DIAGNOSTIC\n");kv("diagnostic_version",VERSION);kv("status","RUNNING");kv("ct_sha256",CT_SHA256);numkv("patch_count",PATCH_COUNT);numkv("fixed_rva_count",FIXED_COUNT);kv("target","re7.exe / RT CT 8.02");text("reframework_plugin_api=");number(g_ref_major);text(".");number(g_ref_minor);text(".");number(g_ref_patch);text("\n");kv("game_memory_writes","NONE");kv("game_file_writes","NONE");kv("network_access","NONE");kv("full_memory_dump","NONE");kv("snapshot","not atomic; live state can change during reads");kv("disk_scan_scope","raw data of mapped PE sections; no overlay/zero-fill/cross-section matches");kv("memory_scan_scope","committed readable non-guarded spans within the main module image");kv("compatibility_warning","BYTE_MATCH_ONLY is not a claim of playable mod compatibility");checkpoint();
 g_buffer=HeapAlloc(GetProcessHeap(),0,CHUNK+DIAG_MAX_PATTERN);if(!g_buffer){kv("error","BUFFER_ALLOCATION_FAILED");goto cleanup;}
 for(u32 i=0;i<PATCH_COUNT;i++){if(!diag_pattern(&g_patterns[i],PATCHES[i].pattern)||!diag_pattern(&g_expected[i],PATCHES[i].expected)){kv("error","INVALID_COMPILED_MANIFEST");goto cleanup;}}
 g_base=(usize)GetModuleHandleW(0);if(!g_base||!read_memory_pe()){kv("error","UNSUPPORTED_OR_UNREADABLE_MAIN_MODULE_PE");goto cleanup;}g_mem_valid=1;
 hexkv("memory_pe_timestamp",g_mem_pe.timestamp);numkv("memory_image_size",g_mem_pe.image_size);hexkv("preferred_image_base",g_mem_pe.preferred_base);
 g_file=CreateFileW(g_exe,GENERIC_READ,7,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
 if(g_file==INVALID_HANDLE){numkv("disk_open_error",GetLastError());}else{LARGE_INTEGER sz;if(GetFileSizeEx(g_file,&sz)&&sz.QuadPart>0&&sz.QuadPart<=DIAG_MAX_IMAGE){g_disk_size=(u64)sz.QuadPart;u32 hn=(u32)(g_disk_size<HEADER_CAP?g_disk_size:HEADER_CAP);if(file_read(0,g_header,hn)&&diag_parse_pe(g_header,hn,g_disk_size,&g_disk_pe)){g_disk_valid=1;numkv("disk_file_size",g_disk_size);hexkv("disk_pe_timestamp",g_disk_pe.timestamp);numkv("disk_image_size",g_disk_pe.image_size);write_status("DISK_SCAN");file_fingerprint();checkpoint();disk_scan();}else kv("disk_pe","INVALID_OR_UNSUPPORTED");}else kv("disk_pe","FILE_TOO_LARGE_OR_SIZE_UNAVAILABLE");}
 kv("phase","MEMORY_SCAN");write_status("MEMORY_SCAN");checkpoint();memory_scan();numkv("disk_scanned_bytes",g_scanned_disk);kv("disk_scan_complete",g_disk_valid&&g_disk_complete?"YES":"NO");numkv("memory_scanned_bytes",g_scanned_memory);numkv("memory_skipped_nonreadable_bytes",g_skipped_memory);numkv("memory_failed_read_bytes",g_failed_memory);kv("memory_readable_scope_complete",g_memory_complete?"YES":"NO");checkpoint();
 write_status("REPORTING");u32 matched=report_sites();fixed_checks();text("\n[SUMMARY]\n");numkv("byte_match_only_count",matched);numkv("other_or_review_count",PATCH_COUNT-matched);kv("object_lifetime_and_scene_transitions","NOT_TESTED");kv("aiming_camera_collision_playability","NOT_TESTED");kv("fixed_pointer_chain_semantics","NOT_TESTED");kv("existing_hook_owner_identification","NOT_PERFORMED; byte differences only");completed=1;
cleanup:
 if(g_log!=INVALID_HANDLE){numkv("elapsed_ms",GetTickCount64()-started);kv("status",completed?"COMPLETE":"ERROR");checkpoint();CloseHandle(g_log);g_log=INVALID_HANDLE;write_status(g_logfailed?"ERROR_LOG_WRITE":completed?"COMPLETE":"ERROR");if(g_logfailed)ref_error("Diagnostic log write failed; result may be incomplete.");else ref_info(completed?"COMPLETE. Report: reframework/data/RE7_OTS_Diagnostics/*.txt":"ERROR. See reframework/data/RE7_OTS_Diagnostics/*.txt");}
 if(g_file!=INVALID_HANDLE){CloseHandle(g_file);g_file=INVALID_HANDLE;}if(g_buffer){HeapFree(GetProcessHeap(),0,g_buffer);g_buffer=0;}__atomic_store_n(&g_busy,0,__ATOMIC_RELEASE);return 0;
}
static void on_present(void){int key=(GetAsyncKeyState(0x78)&0x8000)!=0;
 int was_down=__atomic_exchange_n(&g_keydown,key,__ATOMIC_RELAXED);int request=key&&!was_down;
 if(!__atomic_load_n(&g_once,__ATOMIC_RELAXED)&&__atomic_add_fetch(&g_frames,1,__ATOMIC_RELAXED)>=60){int expected=0;if(__atomic_compare_exchange_n(&g_once,&expected,1,0,__ATOMIC_RELAXED,__ATOMIC_RELAXED))request=1;}if(!request)return;int old=0;if(!__atomic_compare_exchange_n(&g_busy,&old,1,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))return;HANDLE t=CreateThread(0,0,worker,0,0,0);if(!t){__atomic_store_n(&g_busy,0,__ATOMIC_RELEASE);ref_error("CreateThread failed. Press F9 to retry.");return;}CloseHandle(t);ref_info("Diagnostic started; no game patch will be applied.");}
EXPORT void reframework_plugin_required_version(REFVersion*v){v->major=1;v->minor=0;v->patch=0;v->game_name="RE7";}
EXPORT _Bool reframework_plugin_initialize(const REFInit*p){if(g_initialized)return 1;if(!p||!p->functions||!p->functions->on_present||!target_path())return 0;g_ref=p->functions;if(p->version){g_ref_major=p->version->major;g_ref_minor=p->version->minor;g_ref_patch=p->version->patch;}
 /* Remain mapped until process exit: callbacks and worker cannot outlive the DLL. */
 HMODULE self;if(!GetModuleHandleExW(5,(const WCHAR*)&g_module_marker,&self)){ref_error("Unable to pin diagnostic module.");return 0;}
 if(!g_ref->on_present(on_present)){ref_error("Could not register read-only present callback.");return 0;}g_initialized=1;ref_info("Loaded v" VERSION ". Auto scan after rendering begins; F9 rescans while idle.");return 1;}
