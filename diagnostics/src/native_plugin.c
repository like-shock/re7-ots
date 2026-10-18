/* Exact-build original OTS native test plugin. All game writes are confined to
 * native_runtime_dispatch. Worker owns lifecycle/logging; hook callbacks own
 * neither file IO nor REFramework calls. The pinned DLL and trampolines live
 * until process exit, including after partial installation/disable failures. */
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdatomic.h>
#include <math.h>
#include <string.h>
#include <wchar.h>
#include "ref_min.h"
#include "observer.h"
#include "native_runtime.h"
#include "native_options.h"
#include "native_options_ui.h"
#include "MinHook.h"

#ifndef NATIVE_SOURCE_COMMIT
#error "NATIVE_SOURCE_COMMIT must be supplied by native_build.sh"
#endif
#define NATIVE_VERSION "0.1.3"
#define NATIVE_MODE "ORIGINAL_NATIVE_OTS_EXPERIMENTAL"
#define LOG_LIMIT_BYTES (8u * 1024u * 1024u)
#define LOG_END_RESERVE 1024u
#define PATH_CAP 32768
_Static_assert(NATIVE_SITE_COUNT <= NATIVE_MAX_SITES, "detour table capacity");

enum { PHASE_VALIDATING, PHASE_INSTALLING, PHASE_READY, PHASE_BLOCKED };
static const REFFunctions *ref;
static NativeRuntime runtime;
static NativeOptions options;
static SRWLOCK runtime_lock = SRWLOCK_INIT;
static _Atomic int phase, initialization;
static _Atomic unsigned toggle_requests, shoulder_requests;
static _Atomic uint64_t presented_frames;
static DWORD dispatch_tls = TLS_OUT_OF_INDEXES;
static _Atomic int dispatch_fault;
static WCHAR exe_path[PATH_CAP], root[PATH_CAP], folder[PATH_CAP], log_path[PATH_CAP];
static HANDLE logfile = INVALID_HANDLE_VALUE;
static uint64_t base, log_bytes, started, generation, toggles, shoulders;
static uint64_t snapshot_time, snapshot_presented;
static int have_snapshot_time;
static int log_failed, log_full, minhook_initialized;
static unsigned created_hooks;
static char reason[128] = "VALIDATING";
static uint8_t hash_buffer[1024 * 1024], pe_buffer[65536];

int native_options_ui_get(int*value,int*save_status) {
    if(atomic_load_explicit(&phase,memory_order_acquire)!=PHASE_READY)return 0;
    return native_options_get(&options,value,save_status);
}
int native_options_ui_set(unsigned flag,int value) {
    if(atomic_load_explicit(&phase,memory_order_acquire)!=PHASE_READY)return 0;
    return native_options_request_field(&options,flag,value);
}

static int fail(const char *why) {
    snprintf(reason, sizeof reason, "%s", why);
    return 0;
}
static void message(const char *text) {
    if (ref && ref->log_info) ref->log_info("[RE7_NATIVE_OTS] %s", text);
}
static int memory_range(uint64_t address, size_t length) {
    return length != 0 && address >= 0x10000 &&
           address <= UINT64_C(0x7fffffffffff) &&
           length - 1 <= UINT64_C(0x7fffffffffff) - address &&
           address <= (uint64_t)UINTPTR_MAX - (length - 1);
}
static int read_memory(void *context, uint64_t address, void *out, size_t length) {
    (void)context;
    SIZE_T done = 0;
    return out && memory_range(address, length) &&
        ReadProcessMemory(GetCurrentProcess(), (void *)(uintptr_t)address,
                          out, length, &done) && done == length;
}
static int write_memory(void *context, uint64_t address, const void *value, size_t length) {
    (void)context;
    SIZE_T done = 0;
    return value && memory_range(address, length) &&
        WriteProcessMemory(GetCurrentProcess(), (void *)(uintptr_t)address,
                           value, length, &done) && done == length;
}

/* The assembly saves the full CPU state before calling. The second gate check
 * closes a race with worker disable/reset while this dispatch waits for lock.
 * TLS covers lock acquisition too: nested entry must never wait on itself. */
int __attribute__((ms_abi)) native_dispatch(
    unsigned site, NativeFrame *frame, void *extended_state) {
    if (site >= NATIVE_SITE_COUNT || !frame || !extended_state ||
        !atomic_load_explicit(&native_enabled, memory_order_acquire)) return 0;
    DWORD saved_error = GetLastError();
    if (dispatch_tls == TLS_OUT_OF_INDEXES) {
        atomic_store_explicit(&dispatch_fault, 1, memory_order_release);
        atomic_store_explicit(&native_enabled, 0, memory_order_release);
        SetLastError(saved_error);
        return 0;
    }
    void *depth = TlsGetValue(dispatch_tls);
    DWORD tls_error = GetLastError();
    if (depth) { SetLastError(saved_error); return 0; }
    if (tls_error != ERROR_SUCCESS || !TlsSetValue(dispatch_tls, (void *)(uintptr_t)1)) {
        atomic_store_explicit(&dispatch_fault, 1, memory_order_release);
        atomic_store_explicit(&native_enabled, 0, memory_order_release);
        SetLastError(saved_error);
        return 0;
    }
    AcquireSRWLockExclusive(&runtime_lock);
    int handled = 0;
    if (atomic_load_explicit(&native_enabled, memory_order_acquire))
        handled = native_runtime_dispatch(&runtime, site, frame, extended_state);
    ReleaseSRWLockExclusive(&runtime_lock);
    if (!TlsSetValue(dispatch_tls, NULL)) {
        /* This call may already have emulated stores. Retain its handled result
         * while disabling subsequent dispatches; worker performs hook cleanup. */
        atomic_store_explicit(&dispatch_fault, 1, memory_order_release);
        atomic_store_explicit(&native_enabled, 0, memory_order_release);
    }
    SetLastError(saved_error);
    return handled;
}

static int write_log(const char *text, size_t length) {
    DWORD done = 0;
    if (length > LOG_LIMIT_BYTES - log_bytes || logfile == INVALID_HANDLE_VALUE ||
        !WriteFile(logfile, text, (DWORD)length, &done, NULL) || done != length) {
        log_failed = 1;
        return 0;
    }
    log_bytes += done;
    return 1;
}
static void line(const char *format, ...) {
    if (log_failed || log_full) return;
    char buffer[8192];
    va_list args;
    va_start(args, format);
    int length = vsnprintf(buffer, sizeof buffer, format, args);
    va_end(args);
    if (length < 0 || (size_t)length >= sizeof buffer) {
        log_failed = 1;
        return;
    }
    if ((size_t)length > LOG_LIMIT_BYTES - LOG_END_RESERVE - log_bytes) {
        static const char end[] = "{\"type\":\"log_limit\",\"limit_bytes\":8388608,"
            "\"gameplay_continues\":true,\"trampolines_retained\":true}\n";
        log_full = 1;
        write_log(end, sizeof end - 1);
        if (logfile != INVALID_HANDLE_VALUE) FlushFileBuffers(logfile);
        return;
    }
    write_log(buffer, (size_t)length);
}
static int join(WCHAR *out, const WCHAR *prefix, const WCHAR *suffix) {
    size_t a = wcslen(prefix), b = wcslen(suffix);
    if (a + b >= PATH_CAP) return 0;
    memcpy(out, prefix, a * sizeof(WCHAR));
    memcpy(out + a, suffix, (b + 1) * sizeof(WCHAR));
    return 1;
}
static int setup_log(void) {
    DWORD length = GetModuleFileNameW(NULL, exe_path, PATH_CAP);
    if (!length || length >= PATH_CAP) return fail("EXE_PATH_FAILED");
    WCHAR *slash = wcsrchr(exe_path, L'\\');
    if (!slash) slash = wcsrchr(exe_path, L'/');
    if (!slash || _wcsicmp(slash + 1, L"re7.exe")) return fail("WRONG_EXECUTABLE");
    size_t root_length = (size_t)(slash - exe_path);
    memcpy(root, exe_path, root_length * sizeof(WCHAR));
    root[root_length] = 0;
    const WCHAR *directories[] = {
        L"\\reframework", L"\\reframework\\data", L"\\reframework\\data\\RE7_OTS_Native"
    };
    for (unsigned i = 0; i < 3; ++i) {
        if (!join(folder, root, directories[i])) return fail("PATH_TOO_LONG");
        if (!CreateDirectoryW(folder, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
            return fail("LOG_DIRECTORY_FAILED");
    }
    WCHAR suffix[100];
    swprintf(suffix, 100, L"\\native_%llu_%lu.jsonl",
             (unsigned long long)GetTickCount64(), (unsigned long)GetCurrentProcessId());
    if (!join(log_path, folder, suffix)) return fail("PATH_TOO_LONG");
    logfile = CreateFileW(log_path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_NEW,
                         FILE_ATTRIBUTE_NORMAL, NULL);
    return logfile != INVALID_HANDLE_VALUE ? 1 : fail("LOG_OPEN_FAILED");
}
static void load_options(void) {
    WCHAR path[PATH_CAP];char data[64];DWORD count=0;int value=NATIVE_OPTIONS_DEFAULT;
    if(!join(path,folder,L"\\settings.cfg")){native_options_load(&options,NATIVE_OPTIONS_DEFAULT);return;}
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file!=INVALID_HANDLE_VALUE){
        int valid=ReadFile(file,data,sizeof data,&count,NULL)&&native_options_parse(data,count,&value);
        CloseHandle(file);
        if(!valid){value=NATIVE_OPTIONS_DEFAULT;message("Invalid/unreadable settings.cfg; using default native preferences.");}
    }else if(GetLastError()!=ERROR_FILE_NOT_FOUND){
        message("Could not read settings.cfg; using default native preferences.");
    }
    native_options_load(&options,value);
}
static int save_options(int value) {
    WCHAR path[PATH_CAP],temp[PATH_CAP];char data[64];DWORD count=0;
    size_t size=native_options_encode(value,data,sizeof data);
    if(!size||!join(path,folder,L"\\settings.cfg")||!join(temp,folder,L"\\settings.cfg.tmp"))return 0;
    HANDLE file=CreateFileW(temp,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE)return 0;
    int ok=WriteFile(file,data,(DWORD)size,&count,NULL)&&count==size&&FlushFileBuffers(file);
    if(!CloseHandle(file))ok=0;
    if(ok)ok=MoveFileExW(temp,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
    if(!ok)DeleteFileW(temp);
    return ok;
}
/* Runtime lock is held by the caller. F6 resets must retain the UI preference. */
static void reset_runtime(void) {
    int value=NATIVE_OPTIONS_DEFAULT,status=0;native_options_get(&options,&value,&status);
    native_runtime_init(&runtime,base,read_memory,write_memory,NULL);
    native_runtime_set_melee_auto_disable(&runtime,(value&NATIVE_OPTION_MELEE)!=0);
    native_runtime_set_hide_head(&runtime,(value&NATIVE_OPTION_HIDE_HEAD)!=0);
    native_runtime_set_camera_enabled(&runtime,0);
}
/* Worker owns preference/camera transitions; faults always gate dispatch off. */
static void publish_dispatch(int active) {
    unsigned mode=active?1:runtime.hide_head?2:0;
    if(atomic_load_explicit(&dispatch_fault,memory_order_acquire))mode=0;
    atomic_store_explicit(&native_enabled,mode,memory_order_release);
}
static void lua_created(void*state) {
    ref->lock_lua();
    int ok=native_options_ui_register(state);
    ref->unlock_lua();
    message(ok?"Settings available: Script Generated UI > RE7 OTS Native.":
               "Settings UI registration failed; saved native preference remains active.");
}
static int conflicts(void) {
    static const WCHAR *modules[] = {
        L"RE7_OTS_Explore.dll", L"RE7_OTS_NativeProbe.dll", L"RE7_OTS_Observer.dll",
        L"RE7_OTS_Diagnostics.dll"
    };
    static const char *errors[] = {
        "EXPLORE_DLL_LOADED", "PROBE_DLL_LOADED", "OBSERVER_DLL_LOADED", "DIAGNOSTICS_DLL_LOADED"
    };
    for (unsigned i = 0; i < sizeof modules / sizeof modules[0]; ++i)
        if (GetModuleHandleW(modules[i])) return fail(errors[i]);
    WCHAR path[PATH_CAP];
    if (!join(path, root, L"\\reframework\\autorun\\re7_ots_explore.lua")) return fail("PATH_TOO_LONG");
    if (GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES) return fail("EXPLORE_LUA_PRESENT");
    DWORD error = GetLastError();
    return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND
        ? 1 : fail("EXPLORE_LUA_CHECK_FAILED");
}
static int known_build(void) {
    if (!read_memory(NULL, base, pe_buffer, 64)) return fail("PE_READ_FAILED");
    uint32_t nt = diag_u32(pe_buffer + 60);
    if (nt < 64 || nt > sizeof pe_buffer - 24) return fail("PE_HEADER_INVALID");
    if (!read_memory(NULL, base + nt, pe_buffer + nt, 24)) return fail("PE_READ_FAILED");
    uint64_t length = (uint64_t)nt + 24 + diag_u16(pe_buffer + nt + 20) +
                      (uint64_t)diag_u16(pe_buffer + nt + 6) * 40;
    if (length > sizeof pe_buffer || !read_memory(NULL, base, pe_buffer, (size_t)length))
        return fail("PE_READ_FAILED");
    PE image;
    if (!diag_parse_pe(pe_buffer, (size_t)length, 0, &image) ||
        image.timestamp != OBS_TIMESTAMP || image.image_size != OBS_IMAGE_SIZE)
        return fail("UNSUPPORTED_PE");
    HANDLE file = CreateFileW(exe_path, GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    if (file == INVALID_HANDLE_VALUE) return fail("EXE_OPEN_FAILED");
    LARGE_INTEGER size;
    int ok = GetFileSizeEx(file, &size) && size.QuadPart > 0 && size.QuadPart <= DIAG_MAX_IMAGE;
    Sha256 hash;
    diag_sha_init(&hash);
    uint64_t done = 0;
    while (ok && done < (uint64_t)size.QuadPart) {
        DWORD got = 0;
        uint64_t remaining = (uint64_t)size.QuadPart - done;
        DWORD want = remaining > sizeof hash_buffer ? sizeof hash_buffer : (DWORD)remaining;
        if (!ReadFile(file, hash_buffer, want, &got, NULL) || got != want) { ok = 0; break; }
        diag_sha_update(&hash, hash_buffer, got);
        done += got;
    }
    CloseHandle(file);
    if (!ok) return fail("EXE_HASH_READ_FAILED");
    uint8_t digest[32];
    char hex[65];
    diag_sha_final(&hash, digest);
    for (unsigned i = 0; i < 32; ++i) snprintf(hex + i * 2, 3, "%02x", digest[i]);
    return strcmp(hex, OBS_EXE_HASH) == 0 ? 1 : fail("UNSUPPORTED_EXE_HASH");
}
static int verify_sites(void) {
    if (!conflicts()) return 0;
    for (unsigned i = 0; i < NATIVE_SITE_COUNT; ++i) {
        const NativeSiteDefinition *site = &native_sites[i];
        uint8_t bytes[sizeof site->expected];
        MEMORY_BASIC_INFORMATION info;
        uint64_t address = base + site->rva;
        if (site->length < 5 || site->length > sizeof bytes ||
            site->rva >= OBS_IMAGE_SIZE || site->length > OBS_IMAGE_SIZE - site->rva ||
            address < base || !memory_range(address, site->length) ||
            !VirtualQuery((void *)(uintptr_t)address, &info, sizeof info) ||
            info.State != MEM_COMMIT || info.Type != MEM_IMAGE ||
            info.AllocationBase != (void *)(uintptr_t)base ||
            (info.Protect & (PAGE_NOACCESS | PAGE_GUARD)) ||
            !(info.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) ||
            address < (uint64_t)(uintptr_t)info.BaseAddress ||
            address - (uint64_t)(uintptr_t)info.BaseAddress > info.RegionSize ||
            site->length > info.RegionSize - (address - (uint64_t)(uintptr_t)info.BaseAddress) ||
            !read_memory(NULL, address, bytes, site->length) || memcmp(bytes, site->expected, site->length)) {
            snprintf(reason, sizeof reason, "HOOK_BYTES_OR_PROTECTION_%u", i);
            return 0;
        }
    }
    return 1;
}
static int install_hooks(void) {
    atomic_store_explicit(&native_enabled, 0, memory_order_release);
    if (!verify_sites()) return 0;
    MH_STATUS status = MH_Initialize();
    if (status != MH_OK) return fail("MINHOOK_INIT_FAILED");
    minhook_initialized = 1;
    /* All continuation pointers are published before ANY hook can run. */
    for (unsigned i = 0; i < NATIVE_SITE_COUNT; ++i) {
        native_original_ips[i] = (void *)(uintptr_t)(base + native_sites[i].rva);
        native_returns[i] = (void *)(uintptr_t)(base + native_sites[i].rva + native_sites[i].length);
    }
    for (unsigned i = 0; i < NATIVE_SITE_COUNT; ++i) {
        status = MH_CreateHook((void *)(uintptr_t)(base + native_sites[i].rva),
                               native_detours[i], &native_trampolines[i]);
        if (status != MH_OK || !native_trampolines[i]) {
            snprintf(reason, sizeof reason, "CREATE_%u_STATUS_%d", i, status);
            return 0;
        }
        ++created_hooks;
    }
    if (!verify_sites()) return 0;
    for (unsigned i = 0; i < NATIVE_SITE_COUNT; ++i) {
        status = MH_QueueEnableHook((void *)(uintptr_t)(base + native_sites[i].rva));
        if (status != MH_OK) {
            snprintf(reason, sizeof reason, "QUEUE_%u_STATUS_%d", i, status);
            return 0;
        }
    }
    status = MH_ApplyQueued();
    if (status != MH_OK) {
        snprintf(reason, sizeof reason, "ENABLE_STATUS_%d", status);
        return 0;
    }
    return 1;
}
static void block(void) {
    atomic_store_explicit(&native_enabled, 0, memory_order_release);
    atomic_store_explicit(&phase, PHASE_BLOCKED, memory_order_release);
    /* Drain an in-flight dispatcher before MinHook suspends any game threads.
     * Never hold runtime_lock across MinHook (suspended lock owner deadlock). */
    AcquireSRWLockExclusive(&runtime_lock);
    ReleaseSRWLockExclusive(&runtime_lock);
    MH_STATUS status = minhook_initialized ? MH_DisableHook(MH_ALL_HOOKS) : MH_OK;
    line("{\"type\":\"blocked\",\"reason\":\"%s\",\"created_hooks\":%u,"
         "\"disable_status\":%d,\"enabled\":false,\"trampolines_retained\":true}\n",
         reason, created_hooks, status);
    message(reason);
    if (status != MH_OK && status != MH_ERROR_DISABLED)
        message("Hook disable failed. Native writes are gated off; all trampolines remain allocated. Restart the game.");
    /* Do not MH_RemoveHook/MH_Uninitialize/free, even after a failed enable. */
}
static int foreground(void) {
    DWORD pid = 0;
    HWND window = GetForegroundWindow();
    if (window) GetWindowThreadProcessId(window, &pid);
    return pid == GetCurrentProcessId();
}
static void present(void) {
    atomic_fetch_add_explicit(&presented_frames, 1, memory_order_relaxed);
    static int f6_down, f7_down;
    int f6 = (GetAsyncKeyState(VK_F6) & 0x8000) != 0;
    int f7 = (GetAsyncKeyState(VK_F7) & 0x8000) != 0;
    if (atomic_load_explicit(&phase, memory_order_acquire) == PHASE_READY && foreground()) {
        if (f6 && !f6_down) atomic_fetch_add_explicit(&toggle_requests, 1, memory_order_release);
        if (f7 && !f7_down) atomic_fetch_add_explicit(&shoulder_requests, 1, memory_order_release);
    }
    /* Track raw state outside foreground too: refocusing while held is not a new press. */
    f6_down = f6;
    f7_down = f7;
}
static void float_text(char out[32], float value) {
    if (isfinite(value)) snprintf(out, 32, "%.9g", (double)value);
    else memcpy(out, "null", 5);
}
static void snapshot(uint64_t now) {
    NativeSnapshot state;
    AcquireSRWLockExclusive(&runtime_lock);
    native_runtime_snapshot(&runtime, &state);
    ReleaseSRWLockExclusive(&runtime_lock);
    /* Format and write only after releasing the runtime lock. */
    char calls[NATIVE_MAX_SITES * 24], applied[NATIVE_MAX_SITES * 24];
    size_t call_length = 0, applied_length = 0;
    calls[0] = applied[0] = 0;
    for (unsigned i = 0; i < NATIVE_SITE_COUNT; ++i) {
        int a = snprintf(calls + call_length, sizeof calls - call_length, "%s%llu",
                         i ? "," : "", (unsigned long long)state.calls[i]);
        int b = snprintf(applied + applied_length, sizeof applied - applied_length, "%s%llu",
                         i ? "," : "", (unsigned long long)state.applied[i]);
        if (a < 0 || b < 0 || (size_t)a >= sizeof calls - call_length ||
            (size_t)b >= sizeof applied - applied_length) { log_failed = 1; return; }
        call_length += (size_t)a;
        applied_length += (size_t)b;
    }
    uint64_t presentation_time = GetTickCount64();
    uint64_t presentation_frames = atomic_load_explicit(&presented_frames, memory_order_relaxed);
    uint64_t window_ms = have_snapshot_time ? presentation_time - snapshot_time : 0;
    char present_fps[32] = "null";
    if (window_ms) snprintf(present_fps, sizeof present_fps, "%.9g",
        (double)(presentation_frames - snapshot_presented) * 1000.0 / (double)window_ms);
    snapshot_time = presentation_time;
    snapshot_presented = presentation_frames;
    have_snapshot_time = 1;
    char camera[3][32], clip[3][32], fps[32], marker[32]="null";
    if(state.model_marker_available)float_text(marker,state.model_marker);
    for (unsigned i = 0; i < 3; ++i) {
        float_text(camera[i], state.camera[i]);
        float_text(clip[i], state.clip[i]);
    }
    float_text(fps, state.framerate);
    line("{\"type\":\"snapshot\",\"elapsed_ms\":%llu,\"generation\":%llu,\"enabled\":%s,"
         "\"presented_frames\":%llu,\"present_fps\":%s,\"present_window_ms\":%llu,"
         "\"frame\":%u,\"area\":%u,\"fps\":%s,"
         "\"camera_target\":\"0x%llx\",\"ready\":%s,\"input_valid\":\"0x%llx\","
         "\"input_failed\":\"0x%llx\",\"input_observed\":\"0x%llx\","
         "\"firstperson\":%u,\"stopflag\":%u,\"internalfp\":%u,\"aim\":%u,\"blockedgun\":%u,"
         "\"view_changes\":%llu,\"camera_matches\":%llu,\"read_failures\":%llu,\"write_failures\":%llu,"
         "\"toggles\":%llu,\"shoulder_changes\":%llu,\"camera\":[%s,%s,%s],\"clip\":[%s,%s,%s],"
         "\"flashlight_shadow_calls\":%llu,\"flashlight_shadow_applied\":%llu,"
         "\"auto_disable_with_melee_weapons\":%s,\"hide_head_in_first_person\":%s,"
         "\"head_target\":\"0x%llx\",\"head_applied\":[%llu,%llu,%llu],"
         "\"character_raw\":%u,\"characters\":%u,\"character_observed\":%s,"
         "\"model_marker\":%s,\"assumeheadless\":%u,"
         "\"calls\":[%s],\"applied\":[%s]}\n",
         (unsigned long long)(now - started), (unsigned long long)generation,
         atomic_load_explicit(&native_enabled, memory_order_acquire)==1 ? "true" : "false",
         (unsigned long long)presentation_frames, present_fps, (unsigned long long)window_ms,
         state.frame, state.loadarea, fps, (unsigned long long)state.camera_base,
         state.ready ? "true" : "false", (unsigned long long)state.input_valid,
         (unsigned long long)state.input_failed, (unsigned long long)state.input_observed,
         state.firstperson, state.stopflag, state.internalfp, state.aim, state.blockedgun,
         (unsigned long long)state.view_changes, (unsigned long long)state.camera_matches,
         (unsigned long long)state.reads_failed, (unsigned long long)state.writes_failed,
         (unsigned long long)toggles, (unsigned long long)shoulders,
         camera[0], camera[1], camera[2], clip[0], clip[1], clip[2],
         (unsigned long long)state.calls[NS_FLASHLIGHT_SHADOWS_Z_WRITE],
         (unsigned long long)state.applied[NS_FLASHLIGHT_SHADOWS_Z_WRITE],
         state.melee_auto_disable?"true":"false",state.hide_head?"true":"false",
         (unsigned long long)state.head_target,
         (unsigned long long)state.head_applied[0],(unsigned long long)state.head_applied[1],(unsigned long long)state.head_applied[2],
         state.character_raw,state.characters,state.character_observed?"true":"false",marker,state.assumeheadless,calls,applied);
}
static DWORD WINAPI worker(void *unused) {
    (void)unused;
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    atomic_store_explicit(&native_enabled, 0, memory_order_release);
    started = GetTickCount64();
    if (!setup_log()) { block(); return 0; }
    load_options();
    base = (uint64_t)(uintptr_t)GetModuleHandleW(NULL);
    line("{\"type\":\"header\",\"schema\":1,\"version\":\"" NATIVE_VERSION "\","
         "\"mode\":\"" NATIVE_MODE "\",\"source_commit\":\"" NATIVE_SOURCE_COMMIT "\","
         "\"game_data_writes\":true,\"game_writes\":true,\"code_patches\":true,\"face_hiding\":true,"
         "\"starts_enabled\":false,\"limit_bytes\":8388608,\"limit_ms\":null,"
         "\"site_count\":%u,\"build_sha256\":\"" OBS_EXE_HASH "\",\"trampolines_retained\":true}\n",
         (unsigned)NATIVE_SITE_COUNT);
    for (unsigned i = 0; i < NATIVE_SITE_COUNT; ++i)
        line("{\"type\":\"site\",\"index\":%u,\"name\":\"%s\",\"rva\":%u,\"length\":%u}\n",
             i, native_sites[i].name, native_sites[i].rva, native_sites[i].length);
    if (log_failed || !conflicts() || !known_build()) { if (log_failed) fail("LOG_WRITE_FAILED"); block(); goto done; }
    AcquireSRWLockExclusive(&runtime_lock);
    reset_runtime();
    ReleaseSRWLockExclusive(&runtime_lock);
    atomic_store_explicit(&phase, PHASE_INSTALLING, memory_order_release);
    if (!install_hooks()) { block(); goto done; }
    fail("READY_DISABLED");
    atomic_store_explicit(&phase, PHASE_READY, memory_order_release);
    publish_dispatch(0);
    line("{\"type\":\"ready\",\"enabled\":false,\"created_hooks\":%u}\n", created_hooks);
    message("Native OTS ready. Camera starts OFF; saved first-person head hiding applies. F6 toggles camera; F7 changes shoulder. Logs: reframework/data/RE7_OTS_Native.");
    int active = 0, cap_announced = 0;
    uint64_t next_snapshot = 0, next_conflict = 0;
    for (;;) {
        uint64_t now = GetTickCount64();
        if (atomic_load_explicit(&dispatch_fault, memory_order_acquire)) {
            fail("DISPATCH_TLS_FAILED"); block(); break;
        }
        int option_value;unsigned option_token;
        if(native_options_pending(&options,&option_value,&option_token)){
            AcquireSRWLockExclusive(&runtime_lock);
            native_runtime_set_melee_auto_disable(&runtime,(option_value&NATIVE_OPTION_MELEE)!=0);
            native_runtime_set_hide_head(&runtime,(option_value&NATIVE_OPTION_HIDE_HEAD)!=0);
            ReleaseSRWLockExclusive(&runtime_lock);
            publish_dispatch(active);
            int saved=save_options(option_value);
            native_options_complete(&options,option_token,saved);
            line("{\"type\":\"settings\",\"auto_disable_with_melee_weapons\":%s,\"hide_head_in_first_person\":%s,\"saved\":%s}\n",
                 (option_value&NATIVE_OPTION_MELEE)?"true":"false",(option_value&NATIVE_OPTION_HIDE_HEAD)?"true":"false",saved?"true":"false");
            if(!saved)message("Could not save native preferences; session settings are applied.");
        }
        unsigned requested = atomic_exchange_explicit(&toggle_requests, 0, memory_order_acq_rel);
        if (requested & 1u) {
            ++toggles;
            atomic_store_explicit(&native_enabled, 0, memory_order_release);
            if (!active && !conflicts()) { block(); break; }
            AcquireSRWLockExclusive(&runtime_lock);
            if (!active) reset_runtime();
            native_runtime_set_camera_enabled(&runtime,!active);
            ReleaseSRWLockExclusive(&runtime_lock);
            active = !active;
            if (active) {
                ++generation;
                /* Camera generation begins after the serialized reset. */
            }
            publish_dispatch(active);
            line("{\"type\":\"toggle\",\"elapsed_ms\":%llu,\"generation\":%llu,\"enabled\":%s}\n",
                 (unsigned long long)(now - started), (unsigned long long)generation,
                 active ? "true" : "false");
            message(active ? "Original native OTS enabled." : "Native OTS camera OFF; first-person head hiding follows its checkbox.");
        }
        requested = atomic_exchange_explicit(&shoulder_requests, 0, memory_order_acq_rel);
        if (requested & 1u) {
            AcquireSRWLockExclusive(&runtime_lock);
            native_runtime_shoulder(&runtime);
            ReleaseSRWLockExclusive(&runtime_lock);
            ++shoulders;
            line("{\"type\":\"shoulder\",\"elapsed_ms\":%llu,\"generation\":%llu}\n",
                 (unsigned long long)(now - started), (unsigned long long)generation);
        }
        if (now >= next_conflict) {
            if (!conflicts()) { block(); break; }
            next_conflict = now + 1000;
        }
        if (now >= next_snapshot && !log_full) {
            snapshot(now);
            next_snapshot = now + 250;
        }
        if (log_failed) { fail("LOG_WRITE_FAILED"); block(); break; }
        if (log_full && !cap_announced) {
            cap_announced = 1;
            message("Native log reached 8 MB. Logging has stopped; gameplay and F6/F7 controls continue.");
        }
        Sleep(20);
    }
done:
    if (logfile != INVALID_HANDLE_VALUE) {
        FlushFileBuffers(logfile);
        CloseHandle(logfile);
        logfile = INVALID_HANDLE_VALUE;
    }
    return 0;
}
__declspec(dllexport) void reframework_plugin_required_version(REFVersion *version) {
    if (!version) return;
    version->major = 1; version->minor = 0; version->patch = 0; version->game_name = "RE7";
}
__declspec(dllexport) _Bool reframework_plugin_initialize(const REFInit *parameters) {
    int expected = 0;
    if (!atomic_compare_exchange_strong(&initialization, &expected, 1)) return expected == 2;
    if (!parameters || !parameters->functions || !parameters->functions->on_present || !native_cpu_init())
        goto failed;
    ref = parameters->functions;
    native_options_init(&options);
    dispatch_tls = TlsAlloc();
    if (dispatch_tls == TLS_OUT_OF_INDEXES) goto failed;
    /* Never free this TLS slot: pinned detours may outlive logical disable. */
    HMODULE self;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
            (LPCWSTR)(uintptr_t)&reframework_plugin_initialize, &self)) goto failed;
    if (!ref->on_present(present)) goto failed;
    if(!ref->on_lua_created||!ref->lock_lua||!ref->unlock_lua||!ref->on_lua_created(lua_created))
        message("Lua UI callback unavailable; saved native preference remains active.");
    HANDLE thread = CreateThread(NULL, 0, worker, NULL, 0, NULL);
    if (!thread) goto failed;
    CloseHandle(thread);
    atomic_store_explicit(&initialization, 2, memory_order_release);
    message("Native OTS v" NATIVE_VERSION " validating the exact supported build. Starts disabled; no probe prerequisite.");
    return 1;
failed:
    atomic_store_explicit(&native_enabled, 0, memory_order_release);
    atomic_store_explicit(&phase, PHASE_BLOCKED, memory_order_release);
    atomic_store_explicit(&initialization, 3, memory_order_release);
    return 0;
}
