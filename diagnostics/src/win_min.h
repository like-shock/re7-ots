#ifndef RE7_WIN_MIN_H
#define RE7_WIN_MIN_H
#include "core.h"
/* Minimal Windows x64 ABI declarations. No dependency on a bundled Windows SDK.
   Contracts: Microsoft Learn, memoryapi/libloaderapi/fileapi/processthreadsapi.
   Compile with --target=x86_64-pc-windows-msvc. */
typedef void* HANDLE;
typedef void* HMODULE;
typedef u16 WCHAR;
typedef int BOOL;
typedef struct { i64 QuadPart; } LARGE_INTEGER;
typedef struct { u16 year,month,day_of_week,day,hour,minute,second,millisecond; } SYSTEMTIME;
typedef struct { void *BaseAddress,*AllocationBase; u32 AllocationProtect; u16 PartitionId; u16 alignment; usize RegionSize; u32 State,Protect,Type; } MEMORY_BASIC_INFORMATION;
_Static_assert(sizeof(void*)==8,"Windows x64 only");
_Static_assert(sizeof(MEMORY_BASIC_INFORMATION)==48,"VirtualQuery ABI");
_Static_assert(__builtin_offsetof(MEMORY_BASIC_INFORMATION,RegionSize)==24,"VirtualQuery ABI offset");
#ifdef DIAG_HOST_TEST
#define WIN_IMPORT
#else
#define WIN_IMPORT __declspec(dllimport)
#endif
#define WINAPI
WIN_IMPORT HMODULE WINAPI GetModuleHandleW(const WCHAR*);
WIN_IMPORT u32 WINAPI GetModuleFileNameW(HMODULE,WCHAR*,u32);
WIN_IMPORT BOOL WINAPI GetModuleHandleExW(u32,const WCHAR*,HMODULE*);
WIN_IMPORT HANDLE WINAPI GetCurrentProcess(void);
WIN_IMPORT HANDLE WINAPI GetCurrentThread(void);
WIN_IMPORT BOOL WINAPI SetThreadPriority(HANDLE,int);
WIN_IMPORT BOOL WINAPI ReadProcessMemory(HANDLE,const void*,void*,usize,usize*);
WIN_IMPORT usize WINAPI VirtualQuery(const void*,MEMORY_BASIC_INFORMATION*,usize);
WIN_IMPORT HANDLE WINAPI GetProcessHeap(void);
WIN_IMPORT void* WINAPI HeapAlloc(HANDLE,u32,usize);
WIN_IMPORT BOOL WINAPI HeapFree(HANDLE,u32,void*);
WIN_IMPORT HANDLE WINAPI CreateFileW(const WCHAR*,u32,u32,void*,u32,u32,HANDLE);
WIN_IMPORT BOOL WINAPI CreateDirectoryW(const WCHAR*,void*);
WIN_IMPORT BOOL WINAPI ReadFile(HANDLE,void*,u32,u32*,void*);
WIN_IMPORT BOOL WINAPI WriteFile(HANDLE,const void*,u32,u32*,void*);
WIN_IMPORT BOOL WINAPI CloseHandle(HANDLE);
WIN_IMPORT BOOL WINAPI FlushFileBuffers(HANDLE);
WIN_IMPORT BOOL WINAPI GetFileSizeEx(HANDLE,LARGE_INTEGER*);
WIN_IMPORT BOOL WINAPI SetFilePointerEx(HANDLE,LARGE_INTEGER,LARGE_INTEGER*,u32);
WIN_IMPORT void WINAPI GetLocalTime(SYSTEMTIME*);
WIN_IMPORT u64 WINAPI GetTickCount64(void);
WIN_IMPORT u32 WINAPI GetLastError(void);
WIN_IMPORT void WINAPI OutputDebugStringA(const char*);
WIN_IMPORT HANDLE WINAPI CreateThread(void*,usize,u32(WINAPI*)(void*),void*,u32,u32*);
WIN_IMPORT short WINAPI GetAsyncKeyState(int);
#define INVALID_HANDLE ((HANDLE)(usize)~(usize)0)
#define GENERIC_READ 0x80000000u
#define GENERIC_WRITE 0x40000000u
#define OPEN_EXISTING 3u
#define CREATE_NEW 1u
#define FILE_ATTRIBUTE_NORMAL 0x80u
#define MEM_COMMIT 0x1000u
#endif
