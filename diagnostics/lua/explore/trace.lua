-- Scalar-only bounded transition queue. Engine callbacks enqueue; frame/reset flush.
local Trace={}
Trace.__index=Trace
local function finite(n) return type(n)=='number' and n==n and math.abs(n)<=1e15 end
local function snapshot(v,depth,budget)
    if budget.n<=0 then return nil end
    budget.n=budget.n-1
    if type(v)=='boolean' or finite(v) then return v end
    if type(v)=='string' then return v:sub(1,256) end
    if type(v)~='table' or depth==0 then return nil end
    local out={}
    for k,value in pairs(v) do
        if budget.n<=0 then break end
        if (type(k)=='string' and #k<=64) or (finite(k) and k>=1 and k<=32 and k%1==0) then
            out[k]=snapshot(value,depth-1,budget)
        end
    end
    return out
end
function Trace.new(session,run,capacity)
    return setmetatable({session=session,run=run,capacity=capacity or 64,queue={},first=1,size=0,
        previous={},sequence=0,dropped=0},Trace)
end
function Trace:observe(channel,state,t,context)
    if type(channel)~='string' or #channel>32 or type(state)~='string' or #state>96
        or not finite(t) or self.previous[channel]==state then return false end
    local from=self.previous[channel];self.previous[channel]=state;self.sequence=self.sequence+1
    if self.size==self.capacity then
        self.queue[self.first]=nil;self.first=self.first%self.capacity+1
        self.size=self.size-1;self.dropped=self.dropped+1
    end
    local slot=(self.first+self.size-1)%self.capacity+1
    self.queue[slot]={kind='transition',schema=1,session=self.session,run=self.run,event_seq=self.sequence,
        channel=channel,from=from,to=state,lua_clock_s=t,context=snapshot(context,3,{n=64})}
    self.size=self.size+1
    return true
end
function Trace:drain(sink)
    local remaining=self.size
    for _=1,remaining do
        local event=self.queue[self.first]
        event.dropped_total=self.dropped
        local ok,result=pcall(sink,event)
        if not ok or result~=true then return false end
        self.queue[self.first]=nil;self.first=self.first%self.capacity+1;self.size=self.size-1
    end
    return true
end
function Trace:stats()
    return {queued=self.size,dropped_total=self.dropped,event_seq=self.sequence}
end
return Trace
