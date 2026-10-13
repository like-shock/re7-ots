-- Staged model lifecycle; NOT bundled into Explore until a real backend is verified.
-- No SDK calls, managed objects, global object search or guessed bone/mesh names.
-- port.read() -> nil or {id=unique_generation_string, visible=boolean}
-- port.write(id, expected_visible, desired_visible) -> true on guarded write success
-- false means rejected BEFORE a write; an exception may have mutated the target.
-- The backend must re-resolve a verified local-player head-only target and compare
-- identity/value immediately before writing. Boolean visibility alone is not a
-- sufficient backend for several joint values: that backend must own each value.
local Visibility={}
Visibility.__index=Visibility

function Visibility.new(port)
    assert(type(port)=='table' and type(port.read)=='function' and type(port.write)=='function',
        'visibility backend requires read/write functions')
    return setmetatable({port=port,state='WAIT_MODEL',closed=false},Visibility)
end

function Visibility:fail(reason)
    self.fault=self.fault or reason
    self.state=self.fault
    return false
end

function Visibility:read()
    local ok,s=pcall(self.port.read)
    if not ok then self:fail('READ_FAILED');return nil end
    if s==nil then self.state=self.fault or 'WAIT_MODEL';return nil end
    if type(s)~='table' or type(s.id)~='string' or #s.id==0 or #s.id>256
        or type(s.visible)~='boolean' then
        self:fail('INVALID_TARGET');return nil
    end
    if self.identity and self.identity~=s.id then
        self.owned=nil -- Never restore the old baseline onto a replacement.
        self:fail('TARGET_CHANGED');return nil
    end
    self.identity=s.id
    -- Store scalar copies only. A backend may reuse/mutate its snapshot table.
    return {id=s.id,visible=s.visible}
end

function Visibility:change(wanted,cleanup)
    if not cleanup and (self.closed or self.fault) then return false end
    local s=self:read()
    if not s then return false end
    local owned=self.owned
    if owned and s.visible~=owned.last then
        self.owned=nil
        return self:fail('OWNERSHIP_LOST')
    end
    if s.visible==wanted then
        self.state=self.fault or (wanted and 'VISIBLE' or 'HIDDEN')
        return true
    end
    if not owned then
        owned={id=s.id,original=s.visible,last=s.visible}
        self.owned=owned
    end
    -- Even an exception can mutate the target. Read back before reporting failure.
    owned.last=wanted
    local ok,result=pcall(self.port.write,s.id,s.visible,wanted)
    local after=self:read()
    if ok and result==false then
        -- A rejected compare/set does not own changes made by another writer.
        owned.last=s.visible
        if after and after.visible~=s.visible then
            self.owned=nil
            return self:fail('OWNERSHIP_LOST')
        end
        return self:fail('WRITE_FAILED')
    end
    if after and after.visible~=wanted then
        -- An ignored setter and an external write after our setter are ambiguous.
        -- Never adopt that unexplained value as ours, even for later cleanup.
        self.owned=nil
        return self:fail('WRITE_FAILED')
    end
    if after then owned.last=after.visible end
    if not ok or result~=true or not after or after.visible~=wanted then
        return self:fail('WRITE_FAILED')
    end
    self.state=self.fault or (wanted and 'VISIBLE' or 'HIDDEN')
    return true
end

function Visibility:before_camera_restore()
    -- Caller must restore the camera even if this returns false. A head failure
    -- must never hold an unsafe/stale camera offset. Retry owned hiding if possible.
    if self.closed then return false end
    if self.fault and not self.owned then return false end
    return self:change(false,self.fault~=nil)
end

function Visibility:after_camera_apply(confirmed_third_person)
    if confirmed_third_person~=true then return self:before_camera_restore() end
    -- The integration adapter must establish camera readback AND head clearance.
    -- Requested/F6 state or a mere attempted camera write is not confirmation.
    return self:change(true,false)
end

function Visibility:release()
    self.closed=true
    if not self.owned then self.state=self.fault or 'RELEASED';return true end
    local original=self.owned.original
    if not self:change(original,true) then return false end
    self.owned=nil
    self.state=self.fault or 'RELEASED'
    return true
end

return Visibility
