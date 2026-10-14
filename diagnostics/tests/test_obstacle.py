from pathlib import Path
import unittest
from lua_runner import run_lua
from test_ethan_model import module

ROOT=Path(__file__).resolve().parents[1]


class PhysicsTests(unittest.TestCase):
    def check(self,scenario):
        self.assertTrue((ROOT/'lua/explore/physics.lua').exists(),'physics adapter not implemented')
        run_lua(module('explore/core','Core')+module('explore/physics','Physics')+
                (ROOT/'tests/physics_harness.lua').read_text()+scenario)

    def test_nearest_non_player_hit_across_all_layers(self):
        self.check("""
layers[0]={hit(0,0,0.1,child),hit(0,0,0.8,wall)}
layers[32]={hit(0,0,0.4,wall)}
local r=physics:cast({0,0,0},{0,0,1})
assert(r.status=='hit' and math.abs(r.distance-0.4)<1e-8 and r.layer==32)
assert(casts==31 and created==2 and r.contacts==3 and clears==32)
assert(type(r.hit)=='table' and r.hit[3]==0.4)
""")

    def test_clear_ray_and_unknown_owner_are_distinct(self):
        self.check("local r=physics:cast({0,0,0},{0,0,1});assert(r.status=='clear' and casts==31)")
        self.check("layers[0]={hit(0,0,0.3,nil)};assert(physics:cast({0,0,0},{0,0,1}).status=='hit')")

    def test_invalid_contacts_counts_missing_apis_and_cycles_are_unavailable(self):
        cases=["layers[0]={hit(0/0,0,0.1,wall)}", "layers[0]={hit(0.2,0,0.1,wall)}",
               "layers[0]={hit(0,0,-0.2,wall)}", "layers[0]={hit(0,0,1.3,wall)}",
               "contact_override=65", "contact_override=0/0", "cast_error=true",
               "bad_method='enableAllHits'", "bad_method='castRay(via.physics.CastRayQuery, via.physics.CastRayResult)'",
               "player=nil", "layers[0]={hit(0,0,0.3,wall)};wall.call=function() return wall end"]
        for setup in cases:
            with self.subTest(setup=setup):
                self.check(setup+";local r=physics:cast({0,0,0},{0,0,1});assert(r.status=='unavailable');assert(casts<=31)")

    def test_contact_total_budget_and_invalid_endpoints(self):
        self.check("""
for layer=0,32 do layers[layer]={};for i=1,64 do layers[layer][i]=hit(0,0,0.5,wall) end end
local r=physics:cast({0,0,0},{0,0,1});assert(r.status=='unavailable' and casts<=5)
""")
        self.check("assert(physics:cast({0/0,0,0},{0,0,1}).status=='unavailable');assert(casts==0)")


class ObstacleMathTests(unittest.TestCase):
    def check(self,scenario):
        self.assertTrue((ROOT/'lua/explore/obstacle.lua').exists(),'obstacle limiter not implemented')
        run_lua(module('explore/core','Core')+module('explore/obstacle','Obstacle')+
                "local c=Obstacle.new();local p={0,0,0};local target={0,0,1}\n"+scenario)

    def test_inward_is_immediate_and_outward_recovery_is_bounded(self):
        self.check("""
local a,s=c:limit(p,target,{status='clear'},0,'a',0.12);assert(a[3]==1)
a,s=c:limit(p,target,{status='hit',distance=0.5},0.1,'a',0.12)
assert(math.abs(a[3]-0.38)<1e-8 and s=='LIMITED')
a,s=c:limit(p,target,{status='clear'},0.2,'a',0.12);assert(a[3]<=0.531)
a,s=c:limit(p,target,{status='clear'},0.3,'new-camera',0.12);assert(a[3]==1)
""")

    def test_blocked_failed_and_reset_do_not_reuse_old_safe_distance(self):
        self.check("""
local a,s=c:limit(p,target,{status='hit',distance=0.05},0,'a',0.12)
assert(a[3]==0 and s=='BLOCKED')
a,s=c:limit(p,target,{status='unavailable'},0.1,'a',0.12);assert(a==nil and s=='UNAVAILABLE')
c:reset();a,s=c:limit(p,target,{status='clear'},0.2,'a',0.12);assert(a[3]==1)
""")

    def test_invalid_geometry_and_time_never_produce_target(self):
        for result in ["{status='hit',distance=-1}","{status='hit',distance=0/0}","{status='other'}"]:
            self.check("assert(c:limit(p,target,"+result+",0,'a',0.12)==nil)")
        self.check("assert(c:limit(p,target,{status='clear'},0/0,'a',0.12)==nil)")


class ObstacleAdapterTests(unittest.TestCase):
    from test_explore_lua import ExploreAdapterTests
    adapter=ExploreAdapterTests.adapter

    def exercise(self,scenario,enabled=True):
        setup=(ROOT/'tests/physics_harness.lua').read_text().replace('local physics=Physics.new(sdk_physics)','')
        setup+="\nfor k,v in pairs(sdk_physics) do sdk[k]=v end\n"
        setup+="config.collision="+('true' if enabled else 'false')+";config.shoulder=0;config.height=0\n"
        self.adapter(scenario,setup)

    def test_actual_sdk_query_limits_camera_then_recovers_and_aim_restores(self):
        self.exercise("""
start();assert(near(position[3],4) and casts>0)
layers[0]={hit(1,3,3.5,wall)};step();assert(near(position[3],3.38))
layers={};step();assert(position[3]>3.38 and position[3]<=3.531)
step(function(s) s.aim.raw=1 end);assert(near(position[3],3))
for i=1,14 do step() end;assert(near(position[3],4))
phase='reset';callbacks.reset();assert(near(position[3],3))
""")

    def test_blocked_or_unavailable_query_returns_to_base_without_camera_fault(self):
        self.exercise("""
start();layers[0]={hit(1,3,3.05,wall)};step();assert(near(position[3],3))
assert(ui():find('FIRST_PERSON_COLLISION',1,true))
layers={};cast_error=true;step();assert(near(position[3],3))
assert(ui():find('WAIT_COLLISION',1,true));assert(not ui():find('FAULT',1,true))
cast_error=false;for i=1,15 do step() end;assert(near(position[3],4))
key=true;step();key=false;assert(near(position[3],3))
""")

    def test_default_disabled_never_calls_physics(self):
        self.exercise("start();assert(near(position[3],4) and casts==0)",enabled=False)
