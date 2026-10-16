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

    def test_stationary_boundary_jitter_stays_first_person_until_space_is_stable(self):
        self.check("""
c:limit(p,target,{status='hit',distance=0.219},0,'a',0.12)
for i=1,30 do
 local d=i%2==0 and 0.219 or 0.221
 local a,s=c:limit(p,target,{status='hit',distance=d},i*0.02,'a',0.12)
 assert(a[3]==0 and s=='BLOCKED','2mm contact jitter must not toggle FP/OTS')
end
""")

    def test_brief_clear_misses_do_not_release_but_sustained_space_recovers(self):
        self.check("""
c:limit(p,target,{status='hit',distance=0.05},0,'a',0.12)
for i=1,12 do
 local r=i%3==0 and {status='hit',distance=0.05} or {status='clear'}
 local a,s=c:limit(p,target,r,i*0.1,'a',0.12)
 assert(a[3]==0 and s=='BLOCKED','isolated clear rays cannot release')
end
for i=13,16 do
 local a,s=c:limit(p,target,{status='clear'},i*0.1,'a',0.12)
 assert(a[3]==0 and s=='BLOCKED','hold for continuous clear evidence')
end
local a,s=c:limit(p,target,{status='clear'},1.7,'a',0.12)
assert(a[3]>0 and a[3]<=0.151 and s=='LIMITED','resume at bounded outward speed')
for i=18,28 do a,s=c:limit(p,target,{status='clear'},i*0.1,'a',0.12) end
assert(a[3]==1 and s=='CLEAR')
""")

    def test_unavailable_query_requires_stable_recovery_and_gap_is_not_clear_evidence(self):
        self.check("""
c:limit(p,target,{status='clear'},0,'a',0.12)
assert(c:limit(p,target,{status='unavailable'},0.1,'a',0.12)==nil)
local a,s=c:limit(p,target,{status='clear'},0.2,'a',0.12);assert(a[3]==0)
a,s=c:limit(p,target,{status='clear'},1,'a',0.12);assert(a[3]==0,'unobserved gap cannot complete hold')
for i=11,13 do a,s=c:limit(p,target,{status='clear'},i*0.1,'a',0.12);assert(a[3]==0) end
a,s=c:limit(p,target,{status='clear'},1.4,'a',0.12);assert(a[3]>0)
a,s=c:limit(p,target,{status='clear'},1.5,'replacement',0.12);assert(a[3]==1)
""")

    def test_release_at_sixty_fps_can_move_through_sub_threshold_distances(self):
        self.check("""
c:limit(p,target,{status='hit',distance=0.05},0,'a',0.12)
local released=false;local previous=0
for i=1,90 do
 local a,s=c:limit(p,target,{status='clear'},i/60,'a',0.12)
 if a[3]>0 then released=true end
 if released then
  assert(s~='BLOCKED','smooth outward step below 0.1 must not re-enter blocked state')
  assert(a[3]>=previous and a[3]-previous<=1.5/60+1e-9)
 end
 previous=a[3]
end
assert(previous==1 and released)
""")

    def test_short_configured_distance_can_release_and_clock_rollback_restarts_hold(self):
        self.check("""
target={0,0,0.2}
c:limit(p,target,{status='hit',distance=0.05},1,'a',0.12)
c:limit(p,target,{status='clear'},1.1,'a',0.12)
c:limit(p,target,{status='clear'},1.2,'a',0.12)
local a,s=c:limit(p,target,{status='clear'},0.9,'a',0.12);assert(a[3]==0,'clock rollback cannot release')
for i=10,12 do a,s=c:limit(p,target,{status='clear'},i*0.1,'a',0.12);assert(a[3]==0) end
for i=13,17 do a,s=c:limit(p,target,{status='clear'},i*0.1,'a',0.12) end
assert(math.abs(a[3]-0.2)<1e-9,'short distance must not get stuck below a fixed release threshold')
c:reset();a,s=c:limit(p,target,{status='clear'},1.8,'a',0.12);assert(math.abs(a[3]-0.2)<1e-9)
""")

    def test_inward_is_immediate_and_outward_recovery_is_bounded(self):
        self.check("""
local a,s=c:limit(p,target,{status='clear'},0,'a',0.12);assert(a[3]==1)
a,s=c:limit(p,target,{status='hit',distance=0.5},0.1,'a',0.12)
assert(math.abs(a[3]-0.38)<1e-8 and s=='LIMITED')
a,s=c:limit(p,target,{status='clear'},0.2,'a',0.12);assert(a[3]<=0.531)
a,s=c:limit(p,target,{status='clear'},0.3,'new-camera',0.12);assert(a[3]==1)
""")

    def test_blend_limits_rendered_distance_without_latching_a_clear_ray(self):
        self.check("""
local a,s=c:limit(p,target,{status='clear'},0,'a',0.12,0)
assert(a[3]==0 and s~='BLOCKED' and not c:status().blocked)
a,s=c:limit(p,target,{status='clear'},0.1,'a',0.12,1)
assert(a[3]>0 and a[3]<=0.15+1e-9,'cap must include growth of entry blend')
a,s=c:limit(p,target,{status='hit',distance=0.23},0.2,'a',0.12,1)
assert(math.abs(a[3]-0.11)<1e-9,'inward correction remains immediate')
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
        for blend in ["-0.1", "1.1", "0/0", "'bad'", "false"]:
            self.check("assert(c:limit(p,target,{status='clear'},0,'a',0.12,"+blend+")==nil)")


class ObstacleAdapterTests(unittest.TestCase):
    from test_explore_lua import ExploreAdapterTests
    adapter=ExploreAdapterTests.adapter

    def exercise(self,scenario,enabled=True,entry_blend=None):
        setup=(ROOT/'tests/physics_harness.lua').read_text().replace('local physics=Physics.new(sdk_physics)','')
        setup+="\nfor k,v in pairs(sdk_physics) do sdk[k]=v end\n"
        setup+="config.collision="+('true' if enabled else 'false')+";config.shoulder=0;config.height=0\n"
        if entry_blend is not None:
            setup+="config.entry_blend="+str(entry_blend)+"\n"
        self.adapter(scenario,setup)

    def test_stationary_contact_misses_cannot_bounce_camera_or_hide_the_reason(self):
        self.exercise("""
start();layers[0]={hit(1,3,3.05,wall)};step();assert(near(position[3],3))
for i=1,15 do
 layers[0]=i%3==0 and {hit(1,3,3.05,wall)} or nil
 step();assert(near(position[3],3),'stationary hit/clear sequence must remain FP')
end
local observed=false
for _,r in ipairs(records) do
 if r.kind=='transition' and r.to=='FIRST_PERSON_COLLISION' then
  assert(r.context.collision_last.status=='BLOCKED')
  assert(r.context.collision_last.lua_clock_s and r.context.collision_last.stabilization.blocked)
  observed=true
 end
end
assert(observed,'transition must identify collision versus native failure')
layers={};for i=1,15 do step() end;assert(near(position[3],4))
""")

    def test_collision_queries_use_full_destination_during_entry_and_failed_queries(self):
        self.exercise("""
start();key=true;step();key=false;step();key=true;step();key=false
assert(query and near(query.ray[2].z,4),'query endpoint must not depend on entry blend')
cast_error=true;step();assert(near(position[3],3))
cast_error=false;step();assert(near(position[3],3))
step();assert(near(position[3],3),'single successful query cannot end outage recovery')
assert(near(query.ray[2].z,4))
step(function(s) s.aim.raw=1 end);assert(near(position[3],3))
key=true;step();key=false;for i=1,15 do step() end;assert(near(position[3],3),'F6 OFF still wins')
""")

    def test_alternating_query_failures_keep_camera_at_base_without_bypassing_native_gates(self):
        self.exercise("""
start();cast_error=true;step();assert(near(position[3],3))
for i=1,12 do
 cast_error=i%3==0;step();assert(near(position[3],3),'query error hold survives report/reset path')
end
cast_error=false;for i=1,18 do step() end;assert(near(position[3],4))
fail_json=true;step();assert(near(position[3],3))
local found=false
for _,r in ipairs(records) do
 if r.kind=='transition' and r.to=='WAIT_NATIVE_INVALID' then found=true end
end
step();for _,r in ipairs(records) do
 if r.kind=='transition' and r.to=='WAIT_NATIVE_INVALID' then found=true end
end
assert(found,'native failure remains immediate and independently observable')
""")

    def test_long_entry_blend_cannot_exceed_outward_cap_after_query_outage(self):
        self.exercise("""
start();cast_error=true;step();cast_error=false
local previous=position[3]
for i=1,20 do
 step()
 assert(near(query.ray[2].z,4),'blend must not shorten the collision ray')
 assert(position[3]>=previous-1e-9,'clear recovery remains monotone')
 assert(position[3]-previous<=1.5*0.1+1e-9,'final rendered recovery exceeds outward speed cap')
 previous=position[3]
end
assert(near(position[3],4),'long blend must eventually finish')
layers[0]={hit(1,3,3.5,wall)};step();assert(near(position[3],3.38),'inward correction stays immediate')
""",entry_blend=0.8)

    def test_actual_sdk_query_limits_camera_then_recovers_and_aim_restores(self):
        self.exercise("""
start();for i=1,4 do step() end;assert(near(position[3],4) and casts>0)
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
