"""Execute actual worker connection query/consume order with injected XInput."""
from pathlib import Path
import subprocess,tempfile,unittest
from test_native_fov import compile_host
ROOT=Path(__file__).resolve().parents[1]
class ControllerWorkerTests(unittest.TestCase):
 def test_real_worker_reconnect_multislot_missing_provider_and_owner_replacement(self):
  plugin=(ROOT/'src/native_plugin.c').read_text()
  start=plugin.index('static int read_controller_connections(')
  end=plugin.index('static DWORD WINAPI worker(',start)
  functions=plugin[start:end]
  host=r'''#define main adapter_main
#include "native_optional_host.c"
#undef main
#define runtime r
#define DWORD uint32_t
#define ERROR_SUCCESS 0
#define ERROR_DEVICE_NOT_CONNECTED 1167
static int xinput_attempted=1,controller_connection_warning,warnings,runtime_lock;
static unsigned provider_mask=1,provider_error;
static uint32_t provider(uint32_t slot,void*state){memset(state,0,16);return provider_error?5:provider_mask&(1u<<slot)?0:1167;}
static uint32_t(*xinput_get_state)(uint32_t,void*)=provider;
static void initialize_controller_connections(void){CHECK(0);}
static void AcquireSRWLockExclusive(int*lock){CHECK(!*lock);*lock=1;}
static void ReleaseSRWLockExclusive(int*lock){CHECK(*lock);*lock=0;}
static void message(const char*s){CHECK(strstr(s,"F6/F7"));warnings++;}
'''+functions+r'''
static void sample(uint64_t time,unsigned camera,unsigned shoulder){unsigned c,s;controller_worker_sample(time,&c,&s);CHECK(c==camera&&s==shoulder&&!runtime_lock);}
int main(void){
 setup();CHECK(native_runtime_set_controller_bindings(&r,0,-1));sample(20,0,0);press(0,0);press(0,1);
 /* Disconnected devices still produce zeros; query must discard queued press. */
 provider_mask=0;sample(40,0,0);press(0,0);press(0,0);provider_mask=1;sample(60,0,0);press(0,1);sample(80,0,0);press(0,0);press(0,1);sample(100,1,0);
 /* Connecting any other slot invalidates source samples that lack slot IDs. */
 press(0,0);press(0,1);provider_mask=3;sample(120,0,0);press(0,1);sample(140,0,0);press(0,0);press(0,1);sample(160,1,0);
 xinput_get_state=NULL;sample(180,0,0);CHECK(warnings==1);press(0,0);press(0,1);sample(200,0,0);CHECK(warnings==1);
 xinput_get_state=provider;sample(220,0,0);press(0,1);sample(240,0,0);press(0,0);press(0,1);sample(260,1,0);
 provider_error=1;sample(280,0,0);CHECK(warnings==2);
 provider_error=0;provider_mask=1;
 for(unsigned button=0;button<=16;button+=16){
  setup();CHECK(native_runtime_set_controller_bindings(&r,(int)button,(int)button));sample(300,0,0);
  press(button,0);press(button,1);CHECK(r.controller.pending_camera==1&&r.controller.pending_shoulder==1);
  press_owner(button,1,M+0xc000);sample(320,0,0);
  press_owner(button,1,M+0xc000);sample(340,0,0);
  press_owner(button,0,M+0xc000);press_owner(button,1,M+0xc000);sample(360,1,1);
 }
 return 0;
}
'''
  with tempfile.TemporaryDirectory() as directory:
   tmp=Path(directory);(tmp/'worker.c').write_text(host);exe=compile_host(tmp,tmp/'worker.c')
   p=subprocess.run([str(exe)],capture_output=True,text=True);self.assertEqual(p.returncode,0,p.stderr)
