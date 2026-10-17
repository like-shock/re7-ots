import pathlib, unittest
from lua_runner import run_lua

ROOT = pathlib.Path(__file__).resolve().parents[1]

class NativeTransportTests(unittest.TestCase):
    def run_case(self, code):
        run_lua('local Transport=(function()\n' + (ROOT/'lua/explore/transport.lua').read_text() + '\nend)()\n' + code)

    def test_direct_reads_and_clock_never_touch_files(self):
        self.run_case('''
local count=0;local broken=false;local tick=10
local t=Transport.new({api_version=1,read=function() count=count+1;if broken then error('read') end;return '{}' end,
 clock=function() return tick end},{load_file=function() error('file forbidden') end,
 load_string=function(s) assert(s=='{}');return {seq=count} end},'dir/')
assert(t:read().seq==1 and t:now()==10 and t:status().mode=='NATIVE_DIRECT')
broken=true;assert(t:read()==nil and count==2 and t:status().status=='READ_ERROR')
tick=11;assert(t:now()==11)
''')

    def test_explicit_legacy_retry_and_bad_api_fails_closed(self):
        self.run_case('''
local count=0
local j={load_file=function(p) assert(p=='dir/status.json');count=count+1;if count==1 then return nil end;return {seq=2} end}
local t=Transport.new(nil,j,'dir/');assert(t:read().seq==2 and count==2)
assert(t:status().mode=='LEGACY_FILE' and t:status().recovered_total==1)
t=Transport.new({api_version=99},j,'dir/');assert(t:read()==nil and count==2)
assert(t:status().mode=='NATIVE_INCOMPATIBLE')
''')

    def test_bad_payload_and_clock_cannot_authorize_snapshot(self):
        self.run_case('''
local bad=false;local tick=10
local t=Transport.new({api_version=1,read=function() return bad and string.rep('x',8193) or '{}' end,
 clock=function() return tick end},{load_string=function() return {seq=1} end},'')
assert(t:now()==10 and t:read());bad=true;assert(t:read()==nil)
bad=false;tick=9;assert(t:now()==10 and t:read()==nil)
tick=12;assert(t:now()==12 and t:read()==nil,'clock failure latches for this script state')
''')
