#include "native_options.h"
#include <assert.h>
#include <math.h>
#include <string.h>
int main(void){
 const char old[]="auto_disable_with_melee_weapons=0\nhide_head_in_first_person=1\nlimit_light_source_contrast_in_third_person=0\n"
 "hide_mia_hair=0\nforce_custom_filter=1\ncustom_shader=3\nfullbright=1\nfullbright_rgb=8405024\nfullbright_brightness=2.5\ncontroller_camera_button=0\ncontroller_shoulder_button=23\n";
 NativeSettings s;assert(native_settings_parse(old,sizeof old-1,&s));
 assert(s.flags==2&&!s.hide_mia_hair&&s.custom_filter&&s.shader==3&&s.fullbright&&s.rgb==8405024&&s.brightness==2.5f);
 assert(s.camera_distance==1&&s.camera_shoulder==1&&s.camera_height==0);
 assert(!s.use_custom_fov&&s.custom_fov==110&&!s.fp_run_fov&&s.tp_run_fov);
 assert(s.run_fov_amount==20&&s.run_fov_rise==7&&s.run_fov_fall==.7f);
 NativeOptions o;native_options_init(&o);native_settings_load(&o,&s);
 const unsigned ids[]={NATIVE_SETTING_CAMERA_DISTANCE,NATIVE_SETTING_CAMERA_SHOULDER,NATIVE_SETTING_CAMERA_HEIGHT,
 NATIVE_SETTING_CUSTOM_FOV_ENABLED,NATIVE_SETTING_CUSTOM_FOV,NATIVE_SETTING_FP_RUN_FOV,NATIVE_SETTING_TP_RUN_FOV,
 NATIVE_SETTING_RUN_FOV_AMOUNT,NATIVE_SETTING_RUN_FOV_RISE,NATIVE_SETTING_RUN_FOV_FALL};
 const double values[]={1.5,0,-.25,1,95,1,0,30,4,.4};
 for(unsigned j=0;j<10;j++)assert(native_settings_request(&o,ids[j],values[j]));
 NativeSettingsToken first,t;assert(native_settings_pending(&o,&s,&first));
 assert(s.camera_distance==1.5f&&s.camera_shoulder==0&&s.camera_height==-.25f&&s.use_custom_fov&&s.custom_fov==95&&s.fp_run_fov&&!s.tp_run_fov);
 assert(s.run_fov_amount==30&&s.run_fov_rise==4&&s.run_fov_fall==.4f);
 assert(native_settings_request(&o,NATIVE_SETTING_CAMERA_HEIGHT,.25));
 native_settings_complete(&o,&first,1);int status;
 assert(native_settings_get(&o,&s,&status)&&status==0&&s.camera_height==.25f);
 assert(native_settings_pending(&o,&s,&t));native_settings_complete(&o,&t,1);
 const double invalid[]={.49,2.01,.51,2,120.01,-1,1.5,40.01,0,0};
 for(unsigned j=0;j<10;j++){
  assert(!native_settings_request(&o,ids[j],invalid[j]));
  assert(!native_settings_request(&o,ids[j],NAN));
  assert(!native_settings_request(&o,ids[j],INFINITY));
 }
 char buffer[1024];size_t n=native_settings_encode(&s,buffer,sizeof buffer);assert(n&&n<sizeof buffer);
 NativeSettings copy;assert(native_settings_parse(buffer,n,&copy));
 assert(copy.camera_distance==1.5f&&copy.camera_shoulder==0&&copy.camera_height==.25f&&copy.custom_fov==95&&copy.run_fov_fall==.4f&&copy.brightness==2.5f);
 /* An old complete file is accepted; a truncated new record is not. */
 char*last=strstr(buffer,"running_fov_fall_seconds=");assert(last);
 assert(!native_settings_parse(buffer,(size_t)(last-buffer),&copy));
 assert(!native_settings_parse(buffer,n-1,&copy));
 NativeOptions restart;native_options_init(&restart);native_settings_load(&restart,&s);
 assert(native_settings_get(&restart,&copy,&status)&&status==1&&copy.camera_height==.25f&&copy.fp_run_fov&&!copy.tp_run_fov);
 return 0;
}
