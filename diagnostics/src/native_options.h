#ifndef RE7_NATIVE_OPTIONS_H
#define RE7_NATIVE_OPTIONS_H
#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>
#define NATIVE_OPTION_MELEE 1u
#define NATIVE_OPTION_HIDE_HEAD 2u
#define NATIVE_OPTION_LIGHT_CONTRAST 4u
#define NATIVE_OPTIONS_MASK 7u
#define NATIVE_OPTIONS_DEFAULT NATIVE_OPTIONS_MASK
/* Values are a mask of the three independent preferences.
 * UI requests are atomic. One worker loads/persists/completes them; no file
 * IO or game-memory operations occur in the UI or in these helpers. */
typedef struct {
 atomic_uint requested;
 atomic_uint_fast64_t result; /* completed revision plus save-failure bit */
 atomic_int ready;
} NativeOptions;
void native_options_init(NativeOptions*);
void native_options_load(NativeOptions*,int);
int native_options_get(const NativeOptions*,int*value,int*save_status);
int native_options_request(NativeOptions*,int);
int native_options_request_field(NativeOptions*,unsigned flag,int value);
int native_options_pending(const NativeOptions*,int*value,unsigned*token);
void native_options_complete(NativeOptions*,unsigned token,int saved);
int native_options_parse(const char*,size_t,int*);
size_t native_options_encode(int,char*,size_t);
#endif
