#ifndef RE7_REF_MIN_H
#define RE7_REF_MIN_H
/* ABI-compatible prefix declarations, verified against official API.h.
   https://github.com/praydog/REFramework/blob/master/include/reframework/API.h
   Consumers use the existing Lua callback/lock, presentation and log slots. No SDK hook API. */
typedef struct { int major,minor,patch; const char* game_name; } REFVersion;
typedef struct {
 _Bool (*on_lua_created)(void(*)(void*));
 _Bool (*on_lua_destroyed)(void(*)(void*));
 _Bool (*on_present)(void(*)(void));
 _Bool (*on_pre_entry)(const char*,void(*)(void));
 _Bool (*on_post_entry)(const char*,void(*)(void));
 void (*lock_lua)(void);void (*unlock_lua)(void);
 _Bool (*on_device_reset)(void(*)(void));
 _Bool (*on_message)(_Bool(*)(void*, unsigned int, unsigned long long, long long));
 void (*log_error)(const char*,...);
 void (*log_warn)(const char*,...);
 void (*log_info)(const char*,...);
} REFFunctions;
typedef struct { void* module;const REFVersion* version;const REFFunctions* functions;const void* renderer;const void* sdk; } REFInit;
_Static_assert(__builtin_offsetof(REFFunctions,on_present)==16,"REFramework API ABI");
_Static_assert(__builtin_offsetof(REFFunctions,log_info)==88,"REFramework log slot ABI");
_Static_assert(__builtin_offsetof(REFInit,functions)==16,"REFramework init ABI");
#endif
