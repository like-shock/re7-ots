-- Render-time correction for RE7's verified interaction cursor only.
-- Never writes the world anchor or changes interaction selection / firing rays.
local Prompt={}
Prompt.__index=Prompt
local function xyz(v) return v and {v.x,v.y,v.z} or nil end
local function id(o) return o and tostring(o:get_address()) or nil end
local function typed(o,name)
    local ty=o and o:get_type_definition()
    return ty and ty:is_a(name)
end
local function near(a,b,epsilon)
    return Core.vector(a,3,1000000) and Core.vector(b,3,1000000) and Core.distance(a,b)<=epsilon
end
function Prompt.project(world,origin,q,m,width,height)
    if not Core.vector(world,3,1000000) or not Core.vector(origin,3,1000000)
        or not Core.vector(q,4,2) or not Core.finite(width,32768) or not Core.finite(height,32768)
        or width<=0 or height<=0 or type(m)~='table' then return nil end
    local n=q[1]^2+q[2]^2+q[3]^2+q[4]^2
    if n<0.99 or n>1.01 then return nil end
    local k=math.sqrt(n);local x,y,z,w=q[1]/k,q[2]/k,q[3]/k,q[4]/k
    local axes={{1-2*(y*y+z*z),2*(x*y+w*z),2*(x*z-w*y)},
        {2*(x*y-w*z),1-2*(x*x+z*z),2*(y*z+w*x)},
        {2*(x*z+w*y),2*(y*z-w*x),1-2*(x*x+y*y)}}
    local v={0,0,0,1}
    for i=1,3 do for j=1,3 do v[i]=v[i]+axes[i][j]*(world[j]-origin[j]) end end
    if v[3]>=-0.001 then return nil end -- RE camera forward is local -Z.
    local clip={0,0,0,0}
    for c=1,4 do
        if not Core.vector(m[c],4,1000000) then return nil end
        for r=1,4 do clip[r]=clip[r]+m[c][r]*v[c] end
    end
    if not Core.finite(clip[4],1000000) or clip[4]<=0.000001 then return nil end
    local nx,ny=clip[1]/clip[4],clip[2]/clip[4]
    if not Core.finite(nx,1) or not Core.finite(ny,1) then return nil end
    return {(nx+1)*width/2,(1-ny)*height/2,0}
end
function Prompt.new(sdk_api,clock)
    return setmetatable({sdk=sdk_api,clock=clock or os.clock,pending={},state='DISABLED',applied=0},Prompt)
end
function Prompt:configure(enabled,context)
    self.enabled=enabled==true;self.context=context
end
function Prompt:status(detail)
    return {state=self.state,applied=self.applied,evidence=detail and self.evidence or nil}
end
function Prompt:is_candidate(gui)
    -- Include owned controls even if the name changes, so cleanup still runs.
    if self.pending[id(gui)] then return true end
    local go=gui:call('get_GameObject')
    return go and go:call('get_Name')=='InteractOperationCursor'
end
local function resolve(self,gui)
    local go=gui:call('get_GameObject')
    if not go or go:call('get_Name')~='InteractOperationCursor' then return nil end
    local attach=go:call('getComponent(System.Type)',self.sdk.typeof('app.UIWorldPosAttach'))
    if not typed(attach,'app.UIWorldPosAttach') or id(attach:call('get_GameObject'))~=id(go) then return nil end
    local cache=attach:get_field('_TargetGUIElem')
    local control=cache and cache:get_field('_Element')
    if not typed(control,'via.gui.TransformObject') then return nil end
    return {go=go,attach=attach,control=control,view=gui:call('get_View'),
        identity=id(gui)..':'..id(go)..':'..id(attach)..':'..id(control)}
end
local function plain_layout(control,view)
    local parent=control:call('get_Parent')
    local seen={}
    for _=1,16 do
        -- Require a chain reaching this exact view. Nil may mean a missing SDK
        -- method or an unrelated root; neither establishes layout ownership.
        if not parent then return false end
        if id(parent)==id(view) then return true end
        local key=id(parent)
        if seen[key] or not typed(parent,'via.gui.TransformObject') then return false end
        seen[key]=true
        if not near(xyz(parent:call('get_Position')),{0,0,0},0.001)
            or not near(xyz(parent:call('get_Scale')),{1,1,1},0.001) then return false end
        local rot=parent:call('get_Rotation')
        if type(rot)=='number' then if math.abs(rot)>0.001 then return false end
        elseif not near(xyz(rot),{0,0,0},0.001) or (rot.w and math.abs(rot.w-1)>0.001) then return false end
        parent=parent:call('get_Parent')
    end
    return false
end
function Prompt:restore(key)
    local owned=self.pending[key]
    if not owned then return true end
    local ok,done=pcall(function()
        local t=resolve(self,owned.gui)
        if not t or t.identity~=owned.identity then self.state='TARGET_CHANGED';return true end
        local current=xyz(t.control:call('get_Position'))
        if near(current,owned.base,0.001) then return true end
        if not near(current,owned.last,0.001) then self.state='OWNERSHIP_LOST';return true end
        local success=pcall(t.control.call,t.control,'set_Position',Vector3f.new(table.unpack(owned.base)))
        local after=xyz(t.control:call('get_Position'))
        if not success or not near(after,owned.base,0.001) then
            if Core.vector(after,3,1000000) then owned.last=after end
            self.state='RESTORE_FAILED';return false
        end
        return true
    end)
    if ok and done then self.pending[key]=nil;return true end
    self.state='RESTORE_FAILED';return false
end
function Prompt:restore_all()
    local ok=true
    for key in pairs(self.pending) do if not self:restore(key) then ok=false end end
    return ok
end
function Prompt:pre(gui)
    local ok=pcall(function()
        local key=id(gui)
        local existing=self.pending[key]
        if existing then
            local t=resolve(self,gui)
            if t and t.identity==existing.identity and near(xyz(t.control:call('get_Position')),existing.last,0.001) then
                existing.depth=existing.depth+1;return
            end
        end
        if not self:restore(key) then return end
        local c=self.context
        if not self.enabled then self.state='DISABLED';return end
        if not c or self.clock()<c.time or self.clock()-c.time>0.25 then self.state='WAIT_CAMERA';return end
        local t=resolve(self,gui)
        if not t then return end
        if not self.screen_type then
            local ty=self.sdk.find_type_definition('via.gui.ViewType')
            local field=ty and ty:get_field('Screen')
            self.screen_type=field and field:get_data(nil)
        end
        if self.screen_type==nil or not t.view or t.view:call('get_ViewType')~=self.screen_type then
            self.state='UNSUPPORTED_VIEW';return
        end
        local size=t.view:call('get_ScreenSize')
        local world=xyz(t.attach:get_field('_NowTargetPos'))
        local base=xyz(t.control:call('get_Position'))
        local source=Prompt.project(world,c.base,c.rotation,c.projection,size.w,size.h)
        local target=Prompt.project(world,c.target,c.rotation,c.projection,size.w,size.h)
        self.evidence={world=world,control=base,source=source,target=target,serial=c.serial}
        if not source or not target or not Core.vector(base,3,1000000) then self.state='UNPROJECTABLE';return end
        source[3]=base[3];target[3]=base[3]
        if near(base,target,2) then self.state='ALREADY_ALIGNED';return end
        if not near(base,source,2) or not plain_layout(t.control,t.view) then self.state='UNSUPPORTED_LAYOUT';return end
        local count=0;for _ in pairs(self.pending) do count=count+1 end
        if count>=8 then self.state='PENDING_LIMIT';return end
        -- Keep the wrapper only until paired post-draw / next UpdateBehavior cleanup.
        -- Resolve the component/control identities again before any restore.
        local wanted={base[1]+target[1]-source[1],base[2]+target[2]-source[2],base[3]}
        local owned={gui=gui,identity=t.identity,base=base,last=wanted,depth=1}
        self.pending[key]=owned
        local wrote=pcall(t.control.call,t.control,'set_Position',Vector3f.new(table.unpack(wanted)))
        local after=xyz(t.control:call('get_Position'))
        if not wrote or not near(after,wanted,0.001) then
            if Core.vector(after,3,1000000) then owned.last=after end
            self.state='WRITE_FAILED';self:restore(key);return
        end
        self.applied=self.applied+1;self.state='APPLIED'
    end)
    if not ok then self.state='UNAVAILABLE';self:restore_all() end
    return true -- never suppress the original game prompt
end
function Prompt:post(gui)
    local ok,result=pcall(function()
        local key=id(gui);local owned=self.pending[key]
        if owned and owned.depth>1 then owned.depth=owned.depth-1;return true end
        return self:restore(key)
    end)
    return ok and result
end
return Prompt
