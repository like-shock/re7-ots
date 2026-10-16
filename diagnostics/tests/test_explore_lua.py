import pathlib, unittest
from lua_runner import run_lua
ROOT=pathlib.Path(__file__).resolve().parents[1]

FIXTURE="""
local function snapshot(n)
 return {schema=1,version='0.3.0',monitor_version='0.3.1',publication={failures=0,last_error=0},session='re7_explore_2026_10_13_1000_42',phase='MONITORING',build='KNOWN_EXE_HASH_AND_PE',seq=n,tick_ms=1000+n*100,foreground=true,
 player={status='READABLE_CANDIDATE',generation=1,xyz={1,2,3}},
 aim={status='READABLE_CANDIDATE',generation=1,raw=0},
 inventory={status='READABLE_CANDIDATE',generation=1,raw=0},
 interact={status='READABLE_CANDIDATE',slot_hex='0x0',ct_low16_nonzero=false,ct_low32_nonzero=false}}
end
local function warm(g,start)
 for i=1,9 do g:ingest(snapshot(i),start+i*0.1) end
 assert(g:ready(start+0.9))
end
"""

class ExploreCoreTests(unittest.TestCase):
 def core(self,code):
  p=ROOT/'lua/explore/core.lua'
  self.assertTrue(p.exists(),'camera control core not implemented')
  run_lua('local Core=(function()\n'+p.read_text()+'\nend)()\n'+FIXTURE+code)
 def test_stable_heartbeat_and_stale_reset(self):
  self.core("""local g=Core.new();g:ingest(snapshot(1),0.1);assert(not g:ready(0.1))
g:ingest(snapshot(2),0.2);assert(not g:ready(0.69));warm(g,0);assert(not g:ready(1.26))
g:ingest(snapshot(10),1.3);assert(not g:ready(1.3),'stale gap must warm up again')
for i=11,16 do g:ingest(snapshot(i),1.3+(i-10)*0.1) end;assert(g:ready(1.9))
""")
 def test_aim_inventory_interaction_focus_invalid_suspend(self):
  for mutation in ["s.aim.raw=1","s.inventory.raw=1","s.interact.slot_hex='0x100';s.interact.ct_low16_nonzero=true;s.interact.ct_low32_nonzero=true","s.foreground=false","s.player.status='NULL_LINK';s.player.xyz=nil","s.aim.raw=2","s.player.xyz[1]=0/0","s.player.generation='1'","s.interact.ct_low32_nonzero=nil","s.schema=2","s.build='OTHER'","s.version='0.2.0'","s.phase='UNSUPPORTED'"]:
   with self.subTest(mutation=mutation):
    self.core("local g=Core.new();warm(g,0);local s=snapshot(10);"+mutation+";g:ingest(s,1);assert(not g:ready(1));g:ingest(snapshot(11),1.1);assert(not g:ready(1.1))")
 def test_replay_malformed_and_generation_recovery(self):
  self.core("""local g=Core.new();warm(g,0);g:ingest(snapshot(3),1);assert(not g:ready(1),'replay');g:ingest(nil,1.1);assert(not g:ready(1.1));g:ingest(snapshot(10),1.2);assert(not g:ready(1.2))
for i=11,17 do local s=snapshot(i);s.player.generation=2;s.aim.generation=2;g:ingest(s,1.2+(i-10)*0.1);if i==11 then assert(not g:ready(1.3)) end end
assert(g:ready(1.9));local s=snapshot(18);s.player.xyz[1]=100;g:ingest(s,2);assert(not g:ready(2),'teleport');
""")
 def test_terminal_phase_without_new_sample_is_reported(self):
  self.core("""local g=Core.new();local s=snapshot(0);s.phase='VALIDATING';g:ingest(s,0)
s=snapshot(0);s.phase='UNSUPPORTED';s.tick_ms=2000;s.reason='UNSUPPORTED_EXE_HASH';g:ingest(s,1)
assert(not g:ready(1));assert(g.native and g.native.reason=='UNSUPPORTED_EXE_HASH','preserve terminal phase without requiring a sample increment')
""")
 def test_camera_offset_axes_no_accumulation_and_numeric_bounds(self):
  self.core("""local c={schema=1,distance=1,shoulder=0.25,height=0.2};local p={10,20,30}
local r=Core.offset(p,{0,0,0,1},c,1);assert(math.abs(r[1]-10.25)<1e-9 and math.abs(r[2]-20.2)<1e-9 and math.abs(r[3]-31)<1e-9)
local h=math.sqrt(0.5);r=Core.offset(p,{0,h,0,h},c,1);assert(math.abs(r[1]-11)<1e-9 and math.abs(r[3]-29.75)<1e-9)
for i=1,500 do r=Core.offset(p,{0,0,0,1},c,1) end;assert(r[3]==31 and p[3]==30)
assert(Core.offset(p,{0,0,0,0},c,1)==nil);assert(Core.offset({0/0,0,0},{0,0,0,1},c,1)==nil)
local cfg=Core.settings({schema=1,distance=100,shoulder=-9,height=0/0});assert(cfg.distance==2 and cfg.shoulder==-0.6 and cfg.height==0.15)
cfg=Core.settings({schema=999,distance=2});assert(cfg.distance==0.9)
""")

class ExploreJournalTests(unittest.TestCase):
 def journal(self,tail):
  p=ROOT/'lua/explore/journal.lua';self.assertTrue(p.exists(),'bounded journal not implemented')
  run_lua('local Journal=(function()\n'+p.read_text()+'\nend)()\n'+"""
local files={};local bad=false;local closes=0
local io_mock={open=function(path,mode) assert(mode=='wb');files[path]='';return {
write=function(self,line) if bad then return nil end;files[path]=files[path]..line;return self end,
flush=function() return not bad end,close=function() closes=closes+1;return true end} end}
local json_mock={dump_string=function(r) return string.rep('x',r.pad or 80) end}
local j=Journal.new(io_mock,json_mock,'test/',{kind='header'},1024)
"""+tail)
 def test_rotation_keeps_two_files_under_cap(self):
  self.journal("""for i=1,100 do assert(j:record({kind='sample',pad=150})) end;j:close('END');local n=0;for p,s in pairs(files) do n=n+1;assert(#s<=1024,p..' overflow') end;assert(n==2 and closes>2)""")
 def test_failed_write_or_oversized_line_closes(self):
  self.journal("""assert(j:record({kind='sample'}));bad=true;assert(not j:record({kind='sample'}));assert(j.state=='ERROR_WRITE' and closes==1);bad=false;assert(not j:record({kind='sample'}))""")
  self.journal("""assert(not j:record({kind='sample',pad=5000}));assert(j.state=='ERROR_LINE_SIZE')""")

class ExploreAdapterTests(unittest.TestCase):
 def adapter(self,tail,before=''):
  parts=[]
  for name,symbol in [('explore/core','Core'),('explore/journal','Journal'),('explore/trace','Trace'),
                      ('model/visibility','Visibility'),('model/ethan','Ethan'),('model/session','HeadSession'),
                      ('model/group','ModelGroup'),
                      ('explore/physics','Physics'),('explore/obstacle','Obstacle'),
                      ('explore/probe','Probe'),('explore/prompt','Prompt'),
                      ('explore/adapter',None)]:
   p=ROOT/f'lua/{name}.lua';self.assertTrue(p.exists(),name+' not implemented')
   s=p.read_text();parts.append(s if symbol is None else 'local '+symbol+'=(function()\n'+s+'\nend)()')
  harness=(ROOT/'tests/explore_lua_harness.lua').read_text()
  run_lua(FIXTURE+harness.replace('-- INSERT ADAPTER HERE',before+'\n'+'\n'.join(parts))+tail)
 def test_f6_off_is_honored_during_invalid_stale_menu_and_focus_loss(self):
  for change in ["fail_json=true", "menu_open=true", "tick=tick+1", "fixture.foreground=false"]:
   with self.subTest(change=change):
    self.adapter("start();tick=tick+0.1;"+change+""";key=true;phase='frame';callbacks.frame()
phase='update';callbacks.UpdateBehavior();phase='lock';callbacks.LockScene();phase='frame'
key=false;fail_json=false;menu_open=false
for i=1,12 do step() end
assert(near(position[3],3),'OFF must survive recovery')
""")
 def test_repeated_lockscene_preserves_native_fallback_reason(self):
  self.adapter("""start();tick=tick+0.1;seq=seq+1;fixture=snapshot(seq);fixture.aim.raw=1
phase='frame';callbacks.frame();phase='lock';callbacks.LockScene();phase='frame'
assert(near(position[3],3));assert(ui():find('State: FIRST_PERSON_AIM',1,true))
""")
 def test_default_off_enable_follow_and_restore_without_drift(self):
  self.adapter("""for i=1,20 do step() end;assert(set_calls==0,'OFF despite enabled in saved config')
key=true;step();key=false;for i=1,10 do step() end
assert(near(position[1],1.25) and near(position[2],3.2) and near(position[3],4),'camera offset')
for i=1,500 do step() end;assert(near(position[3],4),'no cumulative drift')
local count=set_calls;phase='lock';callbacks.LockScene();phase='frame';assert(set_calls==count,'only one apply per update')
key=true;step();key=false;step();assert(near(position[3],3),'toggle OFF restores')
assert(#records>0,'logs automatic')
""")
 def test_aim_inventory_interaction_and_focus_restore_then_recover(self):
  for mutation in ['s.aim.raw=1','s.inventory.raw=1',"s.interact.slot_hex='0x123';s.interact.ct_low16_nonzero=true;s.interact.ct_low32_nonzero=true",'s.foreground=false']:
   with self.subTest(mutation=mutation):
    self.adapter("start();assert(near(position[3],4));step(function(s) "+mutation+" end);assert(near(position[3],3),'immediate fallback');for i=1,12 do step() end;assert(near(position[3],4),'stable recovery')")
 def test_reload_camera_replacement_and_external_write_not_clobbered(self):
  self.adapter("""start();camera_id=11;transform_id=21;position={1,3,2.5};step();assert(near(position[3],2.5),'old restore must not touch replacement')
for i=1,12 do step() end;assert(near(position[3],3.5))
position={1,3,2.8};key=true;step();key=false;step();assert(near(position[3],2.8),'external position must survive OFF')
""")
 def test_setter_exception_after_mutation_restores_and_latches_off(self):
  self.adapter("""start();phase='update';callbacks.UpdateBehavior();fail_after_set=true;phase='lock';callbacks.LockScene();phase='frame'
assert(near(position[3],3),'failed apply must unwind its own mutation');local n=set_calls;for i=1,12 do step() end;assert(set_calls==n,'fault stays OFF');assert(ui():find('FAULT',1,true))
""")
 def test_restore_failure_and_ignored_setter_are_visible(self):
  self.adapter("""start();fail_set=true;step();assert(ui():find('FAULT',1,true));fail_set=false;step();assert(near(position[3],3));for i=1,12 do step() end;assert(near(position[3],3),'restore failure must not auto-reenable')""")
  self.adapter("""ignore_set=true;start();assert(near(position[3],3));assert(ui():find('FAULT',1,true),'readback detects ignored setter')""")
 def test_partial_failed_apply_recovers_measured_mutation(self):
  self.adapter("""start();phase='update';callbacks.UpdateBehavior();partial_set=true;phase='lock';callbacks.LockScene();phase='frame'
for i=1,10 do step() end;assert(near(position[3],3),'partial application must restore base');assert(ui():find('FAULT',1,true))
""")
 def test_partial_failed_restore_retries_measured_mutation(self):
  self.adapter("""start();partial_restore=true;step();for i=1,10 do step() end
assert(near(position[3],3),'partial restoration must finish at base');assert(ui():find('FAULT',1,true))
""")
 def test_relative_external_writer_cannot_accumulate_offset(self):
  self.adapter("""start();for i=1,10 do position[2]=position[2]+0.01;step();assert(position[3]<=4.002,'residual Z must not become a new base') end
assert(ui():find('FAULT',1,true),'ownership conflict must stay visible')
""")
 def test_stale_malformed_missing_camera_and_log_failure(self):
  self.adapter("""start();fail_json=true;step();assert(near(position[3],3));fail_json=false;for i=1,12 do step() end;assert(near(position[3],4))
nil_camera=true;step();nil_camera=false;position={1,3,3};for i=1,12 do step() end;assert(near(position[3],4));fail_io=true;for i=1,6 do step() end;assert(near(position[3],4),'log failure must not corrupt camera');assert(ui():find('ERROR',1,true))
tick=tick+1;phase='frame';callbacks.frame();phase='update';callbacks.UpdateBehavior();phase='lock';callbacks.LockScene();assert(near(position[3],3))
""")
 def test_malformed_player_cannot_block_fallback_or_reset(self):
  for value in ['17','true','"bad"','nil','{generation={}}']:
   for reset in [False,True]:
    with self.subTest(value=value,reset=reset):
     action=("tick=tick+0.1;seq=seq+1;fixture=snapshot(seq);fixture.player="+value+
             ";phase='frame';callbacks.frame();phase='reset';callbacks.reset()" if reset else
             "step(function(s) s.player="+value+" end)")
     self.adapter("start();assert(near(position[3],4));"+action+"""
assert(near(position[3],3),'invalid player must restore camera with head mode disabled')
assert(not ui():find('FAULT_CALLBACK_EXCEPTION',1,true))
""")
 def test_missing_update_callback_never_applies(self):
  self.adapter("""for i=1,10 do step(nil,true) end;key=true;step(nil,true);key=false;for i=1,15 do step(nil,true) end;assert(set_calls==0);assert(ui():find('CALLBACK',1,true))""")
 def test_registration_failure_stays_blocked_and_reset_restores(self):
  self.adapter("""start();assert(set_calls==0);assert(ui():find('CALLBACK',1,true))""", "re.on_pre_application_entry=function() return false end")
  self.adapter("""start();phase='reset';callbacks.reset();assert(near(position[3],3));assert(saved and saved.enabled==nil,'never persist enabled state')""")
