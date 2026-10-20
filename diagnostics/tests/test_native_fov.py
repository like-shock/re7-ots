"""Real FOV runtime: source cadence, x87 values, frame effects and live authority."""
from pathlib import Path
import platform
import subprocess
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[1]


def compile_host(tmp, host=None, extra=()):
    names = ['native_runtime','native_fullbright','native_controller', 'native_hud', 'native_screen', 'native_av', 'native_head',
             'native_bridge', 'original_scalar', 'original_state', 'original_inputs',
             'original_collision', 'original_interaction', 'original_model']
    if (ROOT/'src/native_fov.c').exists():
        names.append('native_fov')
    bridge = ROOT/'src/native_bridge.S'
    flags = []
    if platform.system() == 'Darwin':
        flags = ['-arch', 'x86_64']
        bridge = tmp/'unexecuted_detours.c'
        bridge.write_text('\n'.join(f'void native_detour_{i}(void){{}}' for i in range(78)))
    exe = tmp/'fov'
    result = subprocess.run(['cc', *flags, '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                             '-ffp-contract=off', '-fno-fast-math', '-I'+str(ROOT/'src'),
                             '-I'+str(ROOT/'tests'), str(host or ROOT/'tests/native_fov_host.c'),
                             *[str(ROOT/'src'/f'{name}.c') for name in names], str(bridge),
                             *map(str, extra), '-lm', '-o', str(exe)], capture_output=True, text=True)
    if result.returncode:
        raise AssertionError(result.stderr)
    return exe


class FovTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.tmp.cleanup)
        cls.exe = compile_host(Path(cls.tmp.name))

    def check_case(self, name):
        result = subprocess.run([str(self.exe), name], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)


for _name in ('menu', 'reference', 'ratio', 'cadence', 'run', 'overshoot', 'authority',
              'rf', 'missing', 'changed', 'race', 'failures', 'reset', 'counts',
              'rollback_owner', 'rf_readiness'):
    setattr(FovTests, 'test_'+_name, lambda self, n=_name: self.check_case(n))


class FovPluginLifecycleTests(unittest.TestCase):
    def test_actual_plugin_reset_retains_copied_menu_and_rf_grace(self):
        # Compile the actual reset function used by the F6 worker, replacing
        # only process memory callbacks. Runtime and preferences remain real.
        plugin = (ROOT/'src/native_plugin.c').read_text()
        start = plugin.index('static void apply_runtime_settings(')
        reset_start = plugin.index('static void reset_runtime(void) {')
        end = plugin.index('\n}', reset_start)+2
        reset_function = plugin[start:end]
        mode_start=plugin.index('static void publish_dispatch(int active) {')
        mode_end=plugin.index('\n}', mode_start)+2
        reset_function+='\nstatic _Atomic int dispatch_fault;\n'+plugin[mode_start:mode_end]
        host = r'''#define main adapter_cases_main
#include "native_fov_host.c"
#undef main
#include "native_options.h"
static NativeOptions options;
static uint64_t base=BASE;
#define runtime r
#define read_memory rd
#define write_memory wr
'''+reset_function+r'''
int main(void){
 native_options_init(&options);native_options_load(&options,NATIVE_OPTIONS_DEFAULT);
 /* Startup has no prior RF write and must apply menu0 on its first frame. */
 setup();memset(&r,0,sizeof r);reset_runtime();native_runtime_set_camera_enabled(&r,1);
 ready();r.inputs.state.loadarea=35;r.view.firstpersonflag=0;
 f.rbx=REF-0x2a8;float initial=72;memcpy(fp+256,&initial,4);CHECK(call("TRUE_FOV_REFERENCE"));
 CHECK(tick());CHECK(get(TARGET)==bits(70));
 setup();producers();CHECK(tick());
 native_runtime_set_camera_enabled(&r,0);reset_runtime();native_runtime_set_camera_enabled(&r,1);
 ready();r.inputs.state.loadarea=35;r.view.firstpersonflag=0;bad_read=MENU;
 f.rbx=REF-0x2a8;float value=72;memcpy(fp+256,&value,4);CHECK(call("TRUE_FOV_REFERENCE"));
 CHECK(tick());CHECK(get(TARGET)==bits(90)); /* menu4 must survive the real reinit */
 f.rdx=TARGET-0x38;value=111;memcpy(fp+192,&value,4);CHECK(call("REFRAMEWORK_FOV_WRITE_POINT"));
 native_runtime_set_camera_enabled(&r,0);reset_runtime();native_runtime_set_camera_enabled(&r,1);
 ready();r.inputs.state.loadarea=35;r.view.firstpersonflag=0;f.rbx=REF-0x2a8;value=72;memcpy(fp+256,&value,4);
 CHECK(call("TRUE_FOV_REFERENCE"));save();CHECK(!tick());unchanged();CHECK(!writes&&get(TARGET)==bits(111));
 save();CHECK(!tick());unchanged();CHECK(!writes&&get(TARGET)==bits(111));
 CHECK(tick());CHECK(get(TARGET)==bits(90));
 CHECK(native_settings_request(&options,NATIVE_SETTING_HAIR,0));
 CHECK(native_settings_request(&options,NATIVE_SETTING_FILTER,1));CHECK(native_settings_request(&options,NATIVE_SETTING_SHADER,15));
 CHECK(native_settings_request(&options,NATIVE_SETTING_FULLBRIGHT,1));CHECK(native_settings_request(&options,NATIVE_SETTING_RGB,0x804020));CHECK(native_settings_request(&options,NATIVE_SETTING_BRIGHTNESS,2.5));
 CHECK(native_settings_request(&options,NATIVE_SETTING_CAMERA_BUTTON,0));CHECK(native_settings_request(&options,NATIVE_SETTING_SHOULDER_BUTTON,23));
 reset_runtime();CHECK(!r.hide_mia_hair&&r.custom_filter&&r.custom_shader==15&&r.fullbright.enabled&&r.fullbright.rgb==0x804020&&r.fullbright.brightness==2.5f&&r.controller.camera_button==0&&r.controller.shoulder_button==23);
 publish_dispatch(0);CHECK(atomic_load(&native_enabled)==12);
 native_runtime_set_hide_head(&r,0);publish_dispatch(0);CHECK(atomic_load(&native_enabled)==11);
 publish_dispatch(1);CHECK(atomic_load(&native_enabled)==1);
 native_runtime_set_controller_bindings(&r,-1,-1);publish_dispatch(0);CHECK(atomic_load(&native_enabled)==8);
 native_runtime_set_hide_head(&r,1);publish_dispatch(0);CHECK(atomic_load(&native_enabled)==10);
 CHECK(native_runtime_set_fullbright(&r,0,0xffffff,1));publish_dispatch(0);CHECK(atomic_load(&native_enabled)==2);
 native_runtime_set_hide_head(&r,0);publish_dispatch(0);CHECK(!atomic_load(&native_enabled));
 CHECK(native_runtime_set_fullbright(&r,1,0xffffff,1));
 atomic_store(&dispatch_fault,1);publish_dispatch(0);CHECK(!atomic_load(&native_enabled));
 atomic_store(&dispatch_fault,1);publish_dispatch(1);CHECK(!atomic_load(&native_enabled));
 puts("actual plugin F6 reinitialization: PASS");return 0;
}
'''
        with tempfile.TemporaryDirectory() as directory:
            tmp = Path(directory)
            (tmp/'plugin_lifecycle.c').write_text(host)
            exe = compile_host(tmp, tmp/'plugin_lifecycle.c', [ROOT/'src/native_options.c'])
            result = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn('actual plugin F6 reinitialization: PASS', result.stdout)
