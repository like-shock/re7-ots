-- Immediate inward movement, bounded outward recovery. No engine objects.
local Obstacle={}
Obstacle.__index=Obstacle
function Obstacle.new() return setmetatable({},Obstacle) end
function Obstacle:reset() self.key=nil;self.time=nil;self.distance=nil end
function Obstacle:limit(base,target,result,time,key,clearance)
    if not Core.vector(base,3,1000000) or not Core.vector(target,3,1000000)
        or not Core.finite(time,9007199254740991) or not Core.finite(clearance,0.3) or clearance<0.03
        or type(key)~='string' or type(result)~='table'
        or (result.status~='clear' and result.status~='hit')
        or (result.status=='hit' and (not Core.finite(result.distance,1000000) or result.distance<0)) then
        self:reset();return nil,'UNAVAILABLE'
    end
    local desired=Core.distance(base,target)
    if desired<0.000001 then self:reset();return {base[1],base[2],base[3]},'BLOCKED' end
    local safe=result.status=='hit' and math.max(0,math.min(desired,result.distance-clearance)) or desired
    if self.key==key and time>=self.time then
        safe=math.min(safe,self.distance+1.5*math.min(0.5,time-self.time))
    end
    self.key=key;self.time=time;self.distance=safe
    if safe<0.1 then return {base[1],base[2],base[3]},'BLOCKED' end
    local adjusted={}
    for i=1,3 do adjusted[i]=base[i]+(target[i]-base[i])*safe/desired end
    return adjusted,safe<desired-0.000001 and 'LIMITED' or 'CLEAR'
end
return Obstacle
