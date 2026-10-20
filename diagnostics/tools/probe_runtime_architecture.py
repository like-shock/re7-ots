#!/usr/bin/env python3
"""Development-only structural probe; this does not measure game FPS.

Runs current production Lua against the existing SDK fixtures. The 400 ms
consumer pause is injected, not a measurement from the user's Steam Deck.
Requires Python 3 and liblua5.4, like the existing offline Lua tests.
"""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tests"))
from lua_runner import run_lua
from test_explore_lua import FIXTURE


def module(name, symbol):
    return "local " + symbol + "=(function()\n" + (ROOT / "lua" / (name + ".lua")).read_text() + "\nend)()\n"


def main():
    core = module("explore/core", "Core")
    models = core + "".join(module(name, symbol) for name, symbol in [
        ("model/visibility", "Visibility"), ("model/ethan", "Ethan"),
        ("model/session", "HeadSession"), ("model/group", "ModelGroup")])
    run_lua(models + (ROOT / "tests/model_backend_harness.lua").read_text() + r'''
local g=ModelGroup.new(sdk,function() return 0 end)
local heads={head,node(head_path)};attach(rig,heads[2])
for i=1,60 do attach(player,node()) end
g:configure(true,'s:1',true);assert(g:before_restore());assert(g:after_apply({1,3,5}))
local function counts()
 local writes=0;for _,n in ipairs(heads) do writes=writes+#n.material_writes end
 return {model_sdk_calls,#model_writes,writes,g:status().search.full_scans}
end
local function report(label,before)
 local after=counts();local delta={}
 for i=1,4 do delta[i]=(after[i]-before[i])/100 end
 print(string.format('%s per frame: SDK=%g DrawSelf writes=%g material writes=%g tree scans=%g',
  label,table.unpack(delta)))
end
local before=counts()
for i=1,100 do
 assert(g:before_restore());assert(g:before_restore());assert(g:after_apply({1,3,5}))
end
report('Legacy OTS hide/show',before)
before=counts()
for i=1,100 do assert(g:after_apply({1,3,5})) end
report('Persistent OTS model path',before)
assert(g:before_restore());before=counts()
for i=1,100 do assert(g:before_restore());assert(g:before_restore()) end
report('FP',before)
''')
    run_lua(core + module("explore/physics", "Physics") +
            (ROOT / "tests/physics_harness.lua").read_text() + r'''
for i=1,100 do assert(physics:cast({0,0,0},{0,0,1}).status=='clear') end
print(string.format('Clear obstacle path per frame: casts=%g query/result objects=%g',casts/100,created/100))
''')
    run_lua(core + FIXTURE + r'''
local g=Core.new();local elapsed_ms,seq=0,0
local function advance(ms)
 elapsed_ms=elapsed_ms+ms;seq=seq+ms/50
 -- The producer advances every 50 ms. The consumer sees only its latest
 -- snapshot, as with the current single status file. No read/write fails.
 local s=snapshot(seq);s.tick_ms=1000+elapsed_ms
 g:ingest(s,elapsed_ms/1000)
 return g:ready(elapsed_ms/1000)
end
for i=1,16 do advance(50) end
assert(g:ready(elapsed_ms/1000))
for cycle=1,5 do
 local ready,reason=advance(400)
 print(string.format('Injected consumer pause %d: 400ms, seq jump=8, ready=%s state=%s cause=%s',
  cycle,tostring(ready),reason,g.reset_evidence.cause))
 for i=1,12 do advance(50) end
 ready,reason=g:ready(elapsed_ms/1000)
 print(string.format('  12 lighter 50ms frames: ready=%s state=%s publication failures=%d',
  tostring(ready),reason,g.native.publication.failures))
end
''')


if __name__ == "__main__":
    main()
