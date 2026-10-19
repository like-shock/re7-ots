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
 atomic_uint_fast64_t optional[8],optional_result[8];
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
/* Independent optional fields use packed 31-bit revision + 32-bit value.
 * One ready gate publishes a complete initial load. Legacy flags have exactly
 * one source of truth: requested/result above, shared with the original API. */
enum {NATIVE_SETTING_HAIR=8,NATIVE_SETTING_FILTER,NATIVE_SETTING_SHADER,
 NATIVE_SETTING_FULLBRIGHT,NATIVE_SETTING_RGB,NATIVE_SETTING_BRIGHTNESS,
 NATIVE_SETTING_CAMERA_BUTTON,NATIVE_SETTING_SHOULDER_BUTTON};
typedef struct {int flags,hide_mia_hair,custom_filter,shader,fullbright;uint32_t rgb;float brightness;int camera_button,shoulder_button;} NativeSettings;
typedef struct {unsigned legacy;uint64_t optional[8];} NativeSettingsToken;
NativeSettings native_settings_defaults(void);
void native_settings_load(NativeOptions*,const NativeSettings*);
int native_settings_get(const NativeOptions*,NativeSettings*,int*);
int native_settings_request(NativeOptions*,unsigned,double);
int native_settings_pending(const NativeOptions*,NativeSettings*,NativeSettingsToken*);
void native_settings_complete(NativeOptions*,const NativeSettingsToken*,int);
int native_settings_parse(const char*,size_t,NativeSettings*);
size_t native_settings_encode(const NativeSettings*,char*,size_t);
#endif
