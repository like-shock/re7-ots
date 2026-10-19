#include "native_options.h"
#include <assert.h>
#include <math.h>
#include <string.h>
int main(void){
 NativeOptions o;native_options_init(&o);NativeSettings s;NativeSettingsToken t;int status;
 assert(!native_settings_get(&o,&s,&status));assert(!native_settings_request(&o,NATIVE_SETTING_SHADER,2));
 const char *old="auto_disable_with_melee_weapons=0\nhide_head_in_first_person=0\nlimit_light_source_contrast_in_third_person=0\n";
 assert(native_settings_parse(old,strlen(old),&s));
 assert(s.flags==0&&s.hide_mia_hair==1&&!s.custom_filter&&s.shader==1&&!s.fullbright&&s.rgb==0xffffff&&s.brightness==1&&s.camera_button==-1&&s.shoulder_button==-1);
 native_settings_load(&o,&s);assert(native_settings_get(&o,&s,&status)&&status==1);
 assert(native_settings_request(&o,NATIVE_SETTING_BRIGHTNESS,2.1234567));assert(native_settings_request(&o,NATIVE_SETTING_CAMERA_BUTTON,0));
 assert(native_settings_pending(&o,&s,&t));NativeSettingsToken first=t;assert(s.camera_button==0&&s.brightness==(float)2.1234567);
 assert(native_settings_request(&o,NATIVE_SETTING_SHADER,15));assert(native_options_request_field(&o,2,1));native_settings_complete(&o,&first,1);
 assert(native_settings_get(&o,&s,&status)&&status==0&&s.shader==15&&s.flags==2&&s.camera_button==0);
 assert(native_settings_pending(&o,&s,&t));native_settings_complete(&o,&t,0);assert(native_settings_get(&o,&s,&status)&&status==-1);assert(!native_settings_pending(&o,&s,&t));
 assert(!native_settings_request(&o,NATIVE_SETTING_SHADER,16));assert(!native_settings_request(&o,NATIVE_SETTING_SHADER,1.5));assert(!native_settings_request(&o,NATIVE_SETTING_BRIGHTNESS,NAN));assert(!native_settings_request(&o,NATIVE_SETTING_BRIGHTNESS,INFINITY));assert(!native_settings_request(&o,NATIVE_SETTING_BRIGHTNESS,10.01));assert(!native_settings_request(&o,NATIVE_SETTING_RGB,0x1000000));assert(!native_settings_request(&o,NATIVE_SETTING_CAMERA_BUTTON,-2));assert(!native_settings_request(&o,NATIVE_SETTING_SHOULDER_BUTTON,24));
 char buffer[1024];size_t n=native_settings_encode(&s,buffer,sizeof buffer);NativeSettings copy;assert(n>108&&n<sizeof buffer);assert(native_settings_parse(buffer,n,&copy));assert(copy.flags==s.flags&&copy.shader==15&&copy.brightness==s.brightness&&copy.camera_button==0);
 assert(!native_settings_parse(buffer,n-1,&copy));buffer[n]='x';assert(!native_settings_parse(buffer,n+1,&copy));
 assert(native_settings_parse(old,34,&copy)&&copy.flags==6);assert(native_settings_parse(old,62,&copy)&&copy.flags==4);
 NativeOptions restart;native_options_init(&restart);native_settings_load(&restart,&s);assert(native_settings_get(&restart,&copy,&status)&&copy.camera_button==0&&copy.shader==15&&copy.brightness==s.brightness);
 return 0;
}
