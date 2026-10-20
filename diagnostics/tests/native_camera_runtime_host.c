#define main original_fov_cases
#include "native_fov_host.c"
#undef main
#include <math.h>
int main(void){
 setup();r.camera_valid=1;
 CHECK(native_runtime_set_camera_preferences(&r,1.5f,.75f,.25f));
 CHECK(r.params.behinddis==1.5f&&r.params.sidedis==.75f&&r.params.yadd==.25f&&!r.camera_valid);
 float y=re7_original_ots_axis(RE7_ORIGINAL_Y,10,1,0,0,&r.params);
 CHECK(y==10.25f&&re7_original_fp_axis(RE7_ORIGINAL_Y,10,1,0,&r.params)==10);
 native_runtime_shoulder(&r);CHECK(r.params.sidedis==-.75f);
 CHECK(native_runtime_set_camera_preferences(&r,1.5f,0,.25f));CHECK(signbit(r.params.sidedis));
 native_runtime_shoulder(&r);CHECK(!signbit(r.params.sidedis));
 native_runtime_shoulder(&r);CHECK(signbit(r.params.sidedis));
 CHECK(native_runtime_set_camera_preferences(&r,1.5f,.5f,.25f));CHECK(r.params.sidedis==-.5f);
 CHECK(!native_runtime_set_camera_preferences(&r,NAN,1,0));
 CHECK(!native_runtime_set_camera_preferences(&r,0,1,0));
 CHECK(!native_runtime_set_camera_preferences(&r,1,1,.51f));
 CHECK(r.params.behinddis==1.5f&&r.params.sidedis==-.5f&&r.params.yadd==.25f);
 setup();producers();CHECK(tick());CHECK(get(TARGET)==bits(90));
 uint32_t age=r.fov.reframeworkfovframes;
 CHECK(native_runtime_set_fov_preferences(&r,1,100,1,0,15,4,1));
 CHECK(!r.fov.active&&!r.fov.reference&&r.fov.reframeworkfovframes==age);
 CHECK(!suppress("FOV_WRITE_MAIN",TARGET));producers();CHECK(tick());CHECK(get(TARGET)==bits(100));
 r.inputs.state.runflag=1;CHECK(tick());CHECK(get(TARGET)==bits(100));
 CHECK(native_runtime_set_fov_preferences(&r,1,100,1,1,15,4,1));
 producers();CHECK(tick());CHECK(get(TARGET)>bits(100));
 /* Unrelated settings replay must not reset an active FOV every UI save. */
 uint64_t target=r.fov.target;float add=r.fov.runningfovadd;
 CHECK(native_runtime_set_fov_preferences(&r,1,100,1,1,15,4,1));
 CHECK(r.fov.active&&r.fov.target==target&&r.fov.runningfovadd==add);
 CHECK(!native_runtime_set_fov_preferences(&r,1,NAN,1,1,15,4,1));
 CHECK(!native_runtime_set_fov_preferences(&r,1,100,1,1,15,0,1));
 CHECK(r.fov.active&&r.fov.runningfovadd==add);
 f.rdx=TARGET-0x38;float rf_value=111;memcpy(fp+192,&rf_value,4);
 CHECK(call("REFRAMEWORK_FOV_WRITE_POINT"));CHECK(r.fov.reframeworkfovframes==0);
 CHECK(native_runtime_set_fov_preferences(&r,1,95,1,1,15,4,1));
 CHECK(r.fov.reframeworkfovframes==0&&!r.fov.active);producers();
 CHECK(!tick());CHECK(!tick());CHECK(tick());
 setup();producers();r.view.firstpersonflag=1;r.inputs.state.runflag=1;
 CHECK(native_runtime_set_fov_preferences(&r,1,100,0,1,15,4,1));producers();
 CHECK(tick());CHECK(get(TARGET)==bits(80));
 CHECK(native_runtime_set_fov_preferences(&r,1,100,1,1,15,4,1));producers();
 CHECK(tick());CHECK(get(TARGET)>bits(80));
 CHECK(native_runtime_set_fov_preferences(&r,1,95,1,1,15,4,1));
 native_runtime_set_camera_enabled(&r,0);save();CHECK(!tick());unchanged();
 CHECK(r.fov.customfovflag&&r.fov.custom_fov==95&&r.fov.fprunfov&&r.fov.tprunfov);
 return 0;
}
