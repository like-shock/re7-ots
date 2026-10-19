#ifndef RE7_NATIVE_OPTIONS_UI_H
#define RE7_NATIVE_OPTIONS_UI_H
#include "native_options.h"
int native_options_ui_get_settings(NativeSettings*,int*);
int native_options_ui_set_setting(unsigned,double);
/* Providers use atomics only. Save status: -1 failed, 0 pending, 1 complete. */
int native_options_ui_get(int*value,int*save_status);
int native_options_ui_set(unsigned flag,int value);
/* Invoke in REFramework's Lua-state-created callback, under its Lua lock. */
int native_options_ui_register(void*state);
#endif
