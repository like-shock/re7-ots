#ifndef RE7_NATIVE_OPTIONS_UI_H
#define RE7_NATIVE_OPTIONS_UI_H
/* Providers use atomics only. Save status: -1 failed, 0 pending, 1 complete. */
int native_options_ui_get(int*value,int*save_status);
int native_options_ui_set(int value);
/* Invoke in REFramework's Lua-state-created callback, under its Lua lock. */
int native_options_ui_register(void*state);
#endif
