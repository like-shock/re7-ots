-- Pure control rules. No SDK calls, files, cached engine objects, or setters.
local M={}
function M.finite(x,bound)
    return type(x)=='number' and x==x and math.abs(x)<=bound
end
local function integer(x,bound) return M.finite(x,bound) and x>=0 and x%1==0 end
function M.vector(p,n,bound)
    if type(p)~='table' then return false end
    for i=1,n do if not M.finite(p[i],bound) then return false end end
    return true
end
function M.distance(a,b)
    local x,y,z=a[1]-b[1],a[2]-b[2],a[3]-b[3]
    return math.sqrt(x*x+y*y+z*z)
end
function M.settings(input)
    local c={schema=1,distance=0.9,shoulder=0.25,height=0.15,ethan_head=false,full_model_pack=false,
        collision=false,collision_clearance=0.12,aim_release_delay=0.5,entry_blend=0.15}
    if type(input)~='table' or input.schema~=1 then return c end
    for _,v in ipairs({{'distance',0.2,2},{'shoulder',-0.6,0.6},{'height',-0.3,0.6},
        {'collision_clearance',0.03,0.3},{'aim_release_delay',0,1.5},{'entry_blend',0.05,0.8}}) do
        local x=input[v[1]]
        if M.finite(x,1000000) then c[v[1]]=math.max(v[2],math.min(v[3],x)) end
    end
    c.ethan_head=input.ethan_head==true
    c.full_model_pack=input.full_model_pack==true
    c.collision=input.collision==true
    return c
end
function M.offset(p,q,c,blend)
    if not M.vector(p,3,1000000) or not M.vector(q,4,2) or not M.finite(blend,1) or blend<0 then return nil end
    local norm=q[1]^2+q[2]^2+q[3]^2+q[4]^2
    if norm<0.5 or norm>1.5 then return nil end
    local k=math.sqrt(norm)
    local x,y,z,w=q[1]/k,q[2]/k,q[3]/k,q[4]/k
    c=M.settings(c)
    -- REFramework camera forward is local -Z, so backward is local +Z.
    local right={1-2*(y*y+z*z),2*(x*y+w*z),2*(x*z-w*y)}
    local back={2*(x*z+w*y),2*(y*z-w*x),1-2*(x*x+y*y)}
    local r={}
    for i=1,3 do r[i]=p[i]+blend*(right[i]*c.shoulder+back[i]*c.distance+(i==2 and c.height or 0)) end
    return M.vector(r,3,1000000) and r or nil
end
local function envelope(s)
    return type(s)=='table' and s.schema==1 and s.version=='0.3.0'
        and type(s.session)=='string' and #s.session<=95
        and s.session:match('^re7_explore_%d+_%d+_%d+_%d+_%d+$')
        and integer(s.seq,9007199254740991) and integer(s.tick_ms,9007199254740991)
        and type(s.foreground)=='boolean' and type(s.phase)=='string'
end
local function health(s)
    if s.phase~='MONITORING' or s.build~='KNOWN_EXE_HASH_AND_PE' then return 'WAIT_NATIVE_'..s.phase end
    if not s.foreground then return 'PAUSED_FOCUS' end
    for _,key in ipairs({'player','aim','inventory'}) do
        local v=s[key]
        if type(v)~='table' or v.status~='READABLE_CANDIDATE' or not integer(v.generation,4294967295) or v.generation==0 then return 'WAIT_'..key end
    end
    if not M.vector(s.player.xyz,3,1000000) then return 'WAIT_POSITION' end
    if s.aim.raw~=0 and s.aim.raw~=1 then return 'WAIT_AIM_VALUE' end
    if s.inventory.raw~=0 and s.inventory.raw~=1 then return 'WAIT_INVENTORY_VALUE' end
    local v=s.interact
    if type(v)~='table' or v.status~='READABLE_CANDIDATE' or type(v.slot_hex)~='string'
        or #v.slot_hex>18 or not v.slot_hex:match('^0x%x+$')
        or type(v.ct_low16_nonzero)~='boolean' or type(v.ct_low32_nonzero)~='boolean' then return 'WAIT_INTERACT' end
    if s.aim.raw==1 then return 'FIRST_PERSON_AIM' end
    if s.inventory.raw==1 then return 'FIRST_PERSON_INVENTORY' end
    if v.ct_low16_nonzero or v.ct_low32_nonzero or not v.slot_hex:match('^0x0+$') then return 'FIRST_PERSON_INTERACTION' end
    return nil
end
function M.new()
    local self={reason='WAIT_NATIVE',native=nil}
    function self:suspend(reason)
        self.stable_at=nil;self.stable_tick=nil;self.reason=reason
    end
    function self:ingest(s,t)
        if not M.finite(t,9007199254740991) or not envelope(s) then
            self.native=nil;self.fresh=false;self.player_step=nil;self:suspend('WAIT_NATIVE_INVALID');return
        end
        local why=health(s)
        if self.session~=s.session then
            self.session=s.session;self.seq=s.seq;self.tick=s.tick_ms;self.changed_at=t
            self.native=s;self.fresh=false;self.generations=nil;self.previous_player=nil
            self.last_aim_at=why=='FIRST_PERSON_AIM' and t or nil
            self:suspend(why or 'WAIT_HEARTBEAT');return
        end
        -- Validation/terminal snapshots have no new gameplay sample number.
        if s.phase~='MONITORING' then
            self.native=s;self.fresh=false;self:suspend(why);return
        end
        if s.seq<self.seq or s.tick_ms<self.tick or ((s.seq==self.seq)~=(s.tick_ms==self.tick)) then
            self.native=nil;self.fresh=false;self:suspend('WAIT_NATIVE_REPLAY');return
        end
        if s.seq==self.seq then
            if why then self.native=s;self:suspend(why) end
            return
        end
        local gap=not self.fresh or t-self.changed_at>0.35 or s.tick_ms-self.tick>350 or t+1e-9<self.changed_at
        self.seq=s.seq;self.tick=s.tick_ms;self.changed_at=t;self.fresh=true;self.native=s
        if why=='FIRST_PERSON_AIM' then self.last_aim_at=t end
        if why then self.previous_player=nil;self.player_step=nil;self:suspend(why);return end
        local generations=s.player.generation..':'..s.aim.generation..':'..s.inventory.generation
        self.player_step=self.previous_player and M.distance(self.previous_player,s.player.xyz) or nil
        local jump=self.player_step and self.player_step>5
        self.previous_player={s.player.xyz[1],s.player.xyz[2],s.player.xyz[3]}
        if gap or generations~=self.generations or jump then self:suspend(jump and 'WAIT_PLAYER_JUMP' or 'WARMUP') end
        self.generations=generations
        if not self.stable_at then self.stable_at=t;self.stable_tick=s.tick_ms end
        self.reason=jump and 'WAIT_PLAYER_JUMP' or 'WARMUP'
    end
    function self:ready(t,settings)
        if not self.fresh or not self.native then return false,self.reason end
        if t+1e-9<self.changed_at or t-self.changed_at>0.35 then
            self.fresh=false;self:suspend('WAIT_NATIVE_STALE');return false,self.reason
        end
        if not self.stable_at then return false,self.reason end
        if t-self.stable_at<0.5-1e-9 or self.tick-self.stable_tick<500 then return false,self.reason end
        local delay=M.settings(settings).aim_release_delay
        if self.last_aim_at and t-self.last_aim_at<delay-1e-9 then
            self.reason='WAIT_AIM_RELEASE';return false,self.reason
        end
        self.reason='READY'
        return true,self.reason
    end
    return self
end
return M
