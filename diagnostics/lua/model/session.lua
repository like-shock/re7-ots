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
function HeadSession:run(fn)
    local ok,result=pcall(fn)
    if not ok then self.fault='MODEL_CALLBACK_ERROR';self.state=self.fault;return false end
    return result
end
function HeadSession:prepare()
    local b=self.backend
    if self.current_epoch and self.current_epoch~=self.epoch then
        -- A generation change invalidates the old baseline, even if all addresses
        -- were reused. Do not restore it into a newly loaded player.
        self.epoch=self.current_epoch;b:set_epoch(self.epoch)
        self.controller=nil;self.fault=nil
    end
    if self.controller and (not self.enabled or self.controller.closed) then
        if not self.controller:release() then
            self.state='RELEASE_'..self.controller.state;return nil
        end
        self.controller=nil;self.fault=nil
    end
    if not self.enabled then self.state='DISABLED';return nil end
    if not self.controller then
        if not self.current_epoch then self.state='WAIT_MODEL_NATIVE';return nil end
        self.controller=Visibility.new({read=function() return b:read_cached() end,
            write=function(id,expected,wanted) return b:write(id,expected,wanted) end})
    end
    return self.controller
end
function HeadSession:before_restore()
    return self:run(function()
        local c=self:prepare()
        if not c then return not self.enabled end
        -- Invalid/stale native input permits hiding an already identified target
        -- only. It cannot authorize acquiring/showing a new head.
        if not self.current_epoch and not c.identity then self.state='WAIT_MODEL_NATIVE';return false end
        local ok=c:before_camera_restore()
        self.state=self.fault or c.state
        return ok
    end)
end
function HeadSession:after_apply(camera_position)
    return self:run(function()
        local c=self.controller
        if not self.enabled or not c or self.fault or not self.current_epoch then return false end
        local head=c.identity and self.backend:head_position(c.identity)
        if not head or not Core.vector(camera_position,3,1000000)
            or Core.distance(head,camera_position)<0.60 then
            local ok=c:before_camera_restore()
            self.state=c.fault or (head and 'HIDDEN_NEAR_CAMERA' or 'WAIT_HEAD_POSITION')
            return ok
        end
        local ok=c:after_camera_apply(true)
        if not ok then c:before_camera_restore() end -- clean up a show that mutated then failed
        self.state=c.state
        return ok
    end)
end
function HeadSession:status()
    return {enabled=self.enabled,state=self.state,binding=self.backend.note,epoch=self.epoch,search=self.backend:stats()}
end
return HeadSession
