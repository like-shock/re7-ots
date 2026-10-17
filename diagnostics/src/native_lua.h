#ifndef RE7_NATIVE_LUA_H
#define RE7_NATIVE_LUA_H
#include <stddef.h>
#define NATIVE_SNAPSHOT_CAP 8192u
/* Provider completes and releases all its locks before the Lua bridge allocates. */
size_t native_read_snapshot(char *out,size_t cap);
double native_clock(void);
int native_lua_register(void *state);
#endif
