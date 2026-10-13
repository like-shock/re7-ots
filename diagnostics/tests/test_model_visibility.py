from pathlib import Path
import unittest
from lua_runner import run_lua

MODULE = Path(__file__).resolve().parents[1] / 'lua/model/visibility.lua'
HARNESS = '''
local visible=true
local identity='session:player-generation:head-A'
local absent=false
local writes,events={},{}
local fail,ignore,mutate_then_throw,read_error=false,false,false,false
local replace_on_write=false
local port={
 read=function()
  if read_error then error('read failed') end
  if absent then return nil end
  return {id=identity,visible=visible}
 end,
 write=function(id,expected,wanted)
  assert(id==identity and expected==visible, 'stale write')
  writes[#writes+1]={id=id,value=wanted}
  events[#events+1]=wanted and 'show' or 'hide'
  if fail then return false end
  if not ignore then visible=wanted end
  if replace_on_write then identity='replacement';visible=true end
  if mutate_then_throw then mutate_then_throw=false;error('changed then threw') end
  return true
 end,
}
local c=Visibility.new(port)
'''


class ModelVisibilityTests(unittest.TestCase):
    def check(self, scenario):
        self.assertTrue(MODULE.exists(), 'model visibility controller not implemented')
        run_lua('local Visibility=(function()\n' + MODULE.read_text() + '\nend)()\n' + HARNESS + scenario)

    def test_show_requires_confirmed_camera_and_fallback_hides_first(self):
        self.check('''
assert(c:before_camera_restore());assert(not visible)
events[#events+1]='camera_restore'
assert(c:after_camera_apply(false));assert(not visible)
events[#events+1]='camera_apply_readback'
assert(c:after_camera_apply(true));assert(visible)
assert(table.concat(events,',')=='hide,camera_restore,camera_apply_readback,show')
assert(c:before_camera_restore());events[#events+1]='aim_camera_restore'
assert(not visible and events[#events-1]=='hide')
assert(c:after_camera_apply(nil));assert(not visible,'nil never enables')
assert(c:after_camera_apply('ACTIVE'));assert(not visible,'truthy strings never enable')
''')

    def test_original_visibility_restored_without_duplicate_writes(self):
        for original in ['true', 'false']:
            with self.subTest(original=original):
                self.check('visible=' + original + '''
assert(c:before_camera_restore());local n=#writes
assert(c:before_camera_restore());assert(#writes==n)
assert(c:after_camera_apply(true));assert(c:release())
assert(visible==''' + original + ''');n=#writes
assert(c:release());assert(#writes==n)
assert(not c:after_camera_apply(true));assert(#writes==n,'closed controller cannot show')
''')

    def test_absent_target_then_acquire_and_temporary_loss_cleanup(self):
        self.check('''
absent=true;assert(not c:before_camera_restore());assert(#writes==0)
absent=false;assert(c:before_camera_restore());assert(not visible)
absent=true;assert(not c:release());assert(not visible)
absent=false;assert(c:release());assert(visible)
''')

    def test_replacement_never_receives_old_baseline(self):
        self.check('''
assert(c:before_camera_restore());identity='other-player';visible=false
local n=#writes;assert(not c:release());assert(#writes==n and not visible)
assert(c.fault=='TARGET_CHANGED')
local next_controller=Visibility.new(port)
assert(next_controller:after_camera_apply(true));assert(visible)
assert(next_controller:release());assert(not visible,'new baseline belongs to new target')
''')
        self.check('''
replace_on_write=true;assert(not c:before_camera_restore());local n=#writes
assert(not c:after_camera_apply(true));c:release();assert(#writes==n)
assert(identity=='replacement' and visible,'replacement survives failed old write')
''')

    def test_external_change_is_preserved_and_latches(self):
        self.check('''
assert(c:before_camera_restore());visible=true;local n=#writes
assert(not c:before_camera_restore());assert(#writes==n and visible)
assert(c.fault=='OWNERSHIP_LOST');assert(not c:after_camera_apply(true))
assert(c:release());assert(#writes==n)
''')

    def test_failed_or_ignored_writes_do_not_claim_success(self):
        for mutation in ['fail=true', 'ignore=true']:
            with self.subTest(mutation=mutation):
                self.check(mutation + '''
assert(not c:before_camera_restore());assert(visible and c.fault=='WRITE_FAILED')
fail=false;ignore=false;local n=#writes
assert(not c:after_camera_apply(true));assert(#writes==n)
assert(c:release());assert(visible)
''')

    def test_exception_after_mutation_retains_cleanup_ownership(self):
        self.check('''
mutate_then_throw=true;assert(not c:before_camera_restore());assert(not visible)
assert(c.fault=='WRITE_FAILED');assert(not c:after_camera_apply(true));assert(not visible)
assert(c:release());assert(visible,'unwind observed failed mutation')
''')

    def test_compare_set_rejection_does_not_adopt_external_mutation(self):
        self.check('''
port.write=function() visible=false;return false end -- another writer wins before CAS
assert(not c:before_camera_restore());assert(c.fault=='OWNERSHIP_LOST')
assert(c:release());assert(not visible,'external mutation must not become our rollback value')
''')

    def test_post_write_external_value_is_not_adopted_for_release(self):
        self.check('''
assert(c:before_camera_restore())
port.write=function(id,expected,wanted)
 assert(id==identity and expected==visible)
 visible=wanted;visible=false -- external hide between successful show and readback
 return true
end
assert(not c:after_camera_apply(true))
port.write=function(id,expected,wanted)
 assert(id==identity and expected==visible);visible=wanted;return true
end
assert(c:release());assert(not visible,'release must preserve the external hide')
''')

    def test_failed_post_write_read_retains_pending_cleanup(self):
        self.check('''
port.write=function(id,expected,wanted)
 assert(id==identity and expected==visible);visible=wanted;read_error=true;return true
end
assert(not c:before_camera_restore());assert(not visible and c.fault=='READ_FAILED')
read_error=false
port.write=function(id,expected,wanted)
 assert(id==identity and expected==visible);visible=wanted;return true
end
assert(c:release());assert(visible)
''')

    def test_failed_release_can_be_retried_but_does_not_reenable(self):
        self.check('''
assert(c:before_camera_restore());fail=true;assert(not c:release());assert(not visible)
fail=false;assert(c:release());assert(visible)
local n=#writes;assert(not c:after_camera_apply(true));assert(#writes==n)
''')

    def test_read_failure_and_invalid_snapshots_never_write(self):
        self.check('read_error=true;assert(not c:before_camera_restore());assert(#writes==0 and c.fault=="READ_FAILED")')
        for value in ['{}', '{id="",visible=true}', '{id="A",visible=1}', '{id={},visible=true}']:
            with self.subTest(value=value):
                self.check('port.read=function() return ' + value + ' end;assert(not c:before_camera_restore());assert(#writes==0 and c.fault=="INVALID_TARGET")')


if __name__ == '__main__':
    unittest.main()
