-- Model failures are isolated from camera recovery. Public engine methods never
-- throw; configuration/status contain only scalar data and perform no SDK calls.
local HeadSession={}
HeadSession.__index=HeadSession
function HeadSession.new(backend)
    return setmetatable({backend=backend,enabled=false,state='DISABLED'},HeadSession)
end
function HeadSession:configure(enabled,epoch)
    self.enabled=enabled==true
    self.current_epoch=type(epoch)=='string' and epoch or nil
end
function HeadSession:invalidate_group()
    if self.backend.invalidate_group then self.backend.invalidate_group() end
end
function HeadSession:run(fn)
    local ok,result
    if self.backend.with_read_scope then ok,result=pcall(self.backend.with_read_scope,self.backend,fn)
    else ok,result=pcall(fn) end
    if not ok or result==false then self:invalidate_group() end
    if not ok then self.fault='MODEL_CALLBACK_ERROR';self.state=self.fault;return false end
    return result
end
function HeadSession:material_change(show,release)
    local success,reason=true,nil
    for _,part in ipairs(self.materials or {}) do
        local c=part.controller
        local ok
        if not release and not self.current_epoch and not c.identity then
            ok=false;c.state='WAIT_MODEL_NATIVE'
        elseif release then ok=c:release()
        elseif show and part.original==true then ok=c:after_camera_apply(true)
        else ok=c:before_camera_restore() end
        if not ok then
            self:invalidate_group()
            success=false;reason=reason or ('MATERIAL_'..part.name..':'..c.state)
            if c.fault then self.fault=self.fault or reason end
        end
    end
    return success,reason
end
function HeadSession:prepare()
    local b=self.backend
    if self.current_epoch and self.current_epoch~=self.epoch then
        -- A generation change invalidates the old baseline, even if all addresses
        -- were reused. Do not restore it into a newly loaded player.
        self.epoch=self.current_epoch;b:set_epoch(self.epoch)
        self.controller=nil;self.materials=nil;self.original_draw=nil;self.fault=nil
    end
    if self.controller and (not self.enabled or self.controller.closed) then
        local materials_ok,reason=self:material_change(false,true)
        local draw_ok=self.controller:release()
        if not draw_ok or not materials_ok then
            self.state='RELEASE_'..(reason or self.controller.state);return nil
        end
        self.controller=nil;self.materials=nil;self.original_draw=nil;self.fault=nil
    end
    if not self.enabled then self.state='DISABLED';return nil end
    if not self.controller then
        if not self.current_epoch then self.state='WAIT_MODEL_NATIVE';return nil end
        local controller
        controller=Visibility.new({read=function()
            local s=b:read_cached()
            if s and self.original_draw==nil then self.original_draw=s.visible end
            return s
        end,
            write=function(id,expected,wanted) return b:write(id,expected,wanted,controller.closed) end})
        self.controller=controller
        self.materials={}
        for _,name in ipairs(b:material_names()) do
            local part={name=name}
            part.controller=Visibility.new({read=function()
                local s=b:read_material(name)
                if s and part.original==nil then part.original=s.visible end
                return s
            end,write=function(id,expected,wanted)
                return b:write_material(id,name,expected,wanted,part.controller.closed)
            end})
            self.materials[#self.materials+1]=part
        end
    end
    return self.controller
end
function HeadSession:before_restore()
    return self:run(function()
        local c=self:prepare()
        if not c then return not self.enabled and self.state=='DISABLED' end
        -- Invalid/stale native input permits hiding an already identified target
        -- only. It cannot authorize acquiring/showing a new head.
        if not self.current_epoch and not c.identity then self.state='WAIT_MODEL_NATIVE';return false end
        if self.backend.preserve_draw and self.original_draw==nil then c:read() end
        -- Each material has independent ownership and an original enable value.
        -- Keep trying the other channels even if one setter fails.
        local materials_ok,reason=self:material_change(false,false)
        local ok=c:before_camera_restore()
        self.state=self.fault or reason or c.state
        return ok and materials_ok
    end)
end
function HeadSession:after_apply(camera_position)
    return self:run(function()
        local c=self.controller
        if not self.enabled or not c or self.fault or not self.current_epoch then return false end
        local head=c.identity and self.backend:head_position(c.identity)
        if not head or not Core.vector(camera_position,3,1000000)
            or Core.distance(head,camera_position)<0.60 then
            self:invalidate_group()
            local materials_ok,reason=self:material_change(false,false)
            local ok=c:before_camera_restore()
            self.state=self.fault or reason or c.fault or (head and 'HIDDEN_NEAR_CAMERA' or 'WAIT_HEAD_POSITION')
            return ok and materials_ok
        end
        local ok
        if self.backend.preserve_draw then
            if self.original_draw==nil then self.state='WAIT_MODEL_DRAW_BASELINE';return false end
            ok=c:change(self.original_draw,false)
        else ok=c:after_camera_apply(true) end
        local materials_ok,reason=false,nil
        if ok then materials_ok,reason=self:material_change(true,false) end
        if not ok or not materials_ok then
            self:invalidate_group()
            self:material_change(false,false);c:before_camera_restore()
        end
        self.state=self.fault or reason or (ok and materials_ok and self.backend.preserve_draw and 'RESTORED_BASELINE') or c.state
        return ok and materials_ok
    end)
end
function HeadSession:status(detail)
    local materials
    if detail then
        materials={}
        for _,part in ipairs(self.materials or {}) do
            materials[part.name]={state=part.controller.state,original=part.original}
        end
    end
    return {enabled=self.enabled,state=self.state,binding=self.backend.note,epoch=self.epoch,
        diagnostic_kind=self.backend.diagnostic and self.backend.diagnostic.kind or nil,
        diagnostic=detail and self.backend.diagnostic or nil,
        target=detail and self.backend.target_info or nil,
        original_draw=detail and self.original_draw,
        materials=materials,draw=detail and self.backend.draw or nil,search=self.backend:stats()}
end
return HeadSession
