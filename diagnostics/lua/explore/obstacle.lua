-- Immediate inward movement, hysteresis and continuous evidence before leaving
-- a blocked/unavailable state, then bounded outward recovery. No engine objects.
local Obstacle={}
Obstacle.__index=Obstacle
local BLOCK_DISTANCE,RELEASE_DISTANCE,RELEASE_HOLD,MAX_SAMPLE_GAP=0.1,0.2,0.4,0.25
function Obstacle.new() return setmetatable({},Obstacle) end
function Obstacle:reset()
    self.key=nil;self.time=nil;self.distance=nil;self.blocked=nil
    self.release_since=nil;self.hold_reason=nil
end
function Obstacle:status()
    return {blocked=self.blocked==true,distance=self.distance,hold_reason=self.hold_reason or 'NONE',
        release_since=self.release_since,release_hold_s=RELEASE_HOLD}
end
function Obstacle:limit(base,target,result,time,key,clearance,blend)
    if blend==nil then blend=1 end
    if not Core.vector(base,3,1000000) or not Core.vector(target,3,1000000)
        or not Core.finite(time,9007199254740991) or time<0
        or not Core.finite(blend,1) or blend<0
        or not Core.finite(clearance,0.3) or clearance<0.03 or type(key)~='string' then
        self:reset();return nil,'UNAVAILABLE'
    end
    if self.key~=key then self:reset() end
    local previous_time=self.time
    if not previous_time or time<previous_time or time-previous_time>MAX_SAMPLE_GAP then
        self.release_since=nil -- Missing observations are not evidence of space.
    end
    self.key=key;self.time=time
    if type(result)~='table' or (result.status~='clear' and result.status~='hit')
        or (result.status=='hit' and (not Core.finite(result.distance,1000000) or result.distance<0)) then
        self.blocked=true;self.distance=0;self.release_since=nil;self.hold_reason='QUERY_UNAVAILABLE'
        return nil,'UNAVAILABLE'
    end
    local desired=Core.distance(base,target)
    local safe=result.status=='hit' and math.max(0,math.min(desired,result.distance-clearance)) or desired
    if safe<BLOCK_DISTANCE then
        self.blocked=true;self.release_since=nil;self.hold_reason='WALL'
    end
    if self.blocked then
        -- Use un-smoothed available space for release, not the held output of
        -- zero. A shorter configured camera distance must still be releasable.
        if safe>=BLOCK_DISTANCE and safe+1e-9>=math.min(RELEASE_DISTANCE,desired) then
            self.release_since=self.release_since or time
            if time-self.release_since>=RELEASE_HOLD-1e-9 then
                self.blocked=false;self.release_since=nil;self.hold_reason=nil
            end
        else self.release_since=nil end
        if self.blocked then
            self.distance=0;return {base[1],base[2],base[3]},'BLOCKED'
        end
    end
    -- Limit the final displacement, including entry blend. Multiplying after
    -- the outward cap would let a growing blend exceed that speed. Release
    -- evidence above still uses the full unblended available space.
    safe=math.min(safe,desired*blend)
    if self.distance and previous_time then
        safe=math.min(safe,self.distance+1.5*math.min(0.5,math.max(0,time-previous_time)))
    end
    self.distance=safe
    local adjusted={}
    for i=1,3 do adjusted[i]=base[i]+(target[i]-base[i])*safe/desired end
    return adjusted,safe<desired-0.000001 and 'LIMITED' or 'CLEAR'
end
return Obstacle
