-- Per-piece ownership for supported RT model families. This facade never keeps SDK objects.
local Group={}
Group.__index=Group
function Group.new(sdk_api,clock)
    return setmetatable({sdk=sdk_api,clock=clock or os.clock,parts={},state='DISABLED',
        enabled=false,scans=0},Group)
end
function Group:configure(enabled,epoch,full_pack)
    self.enabled=enabled==true;self.current_epoch=epoch
    self.requested_mode=full_pack==true
end
function Group:run(fn)
    local ok,result=pcall(fn)
    if not ok then self.fault='MODEL_CALLBACK_ERROR';self.state=self.fault;return false end
    return result
end
function Group:valid_group()
    self.scans=self.scans+1
    local group,reason,diagnostic=Ethan.discover(self.sdk,self.current_epoch)
    self.diagnostic=diagnostic
    if not group or group.identity~=self.identity then
        self.fault=group and 'TARGET_CHANGED' or reason;return false
    end
    return true
end
function Group:hide_all()
    local ok=true
    for _,part in ipairs(self.parts) do
        part.session:configure(self.enabled,self.current_epoch)
        if not part.session:before_restore() then ok=false;self.state=part.session.state end
        local s=part.session
        if s.fault or (s.controller and s.controller.fault) then self.fault=s.state end
    end
    return ok
end
function Group:before_restore()
    return self:run(function()
        if self.current_epoch and self.current_epoch~=self.epoch then
            self.parts={};self.epoch=self.current_epoch;self.fault=nil;self.identity=nil;self.retry_at=nil;self.family=nil;self.diagnostic=nil
        end
        if self.mode~=self.requested_mode then
            local released=true
            for _,part in ipairs(self.parts) do
                part.session:configure(false,self.current_epoch)
                if not part.session:before_restore() then released=false;self.state=part.session.state end
            end
            if not released then return false end
            self.parts={};self.mode=self.requested_mode;self.fault=nil;self.identity=nil;self.retry_at=nil;self.family=nil;self.diagnostic=nil
        end
        if not self.enabled then
            local ok=self:hide_all()
            if ok then self.parts={};self.fault=nil;self.identity=nil;self.retry_at=nil;self.family=nil;self.diagnostic=nil;self.state='DISABLED' end
            return ok
        end
        if #self.parts==0 then
            if not self.current_epoch then self.state='WAIT_MODEL_NATIVE';self.diagnostic=nil;return false end
            local names={'ethan'}
            if self.mode then
                if self.retry_at and self.clock()<self.retry_at then return false end
                self.retry_at=self.clock()+0.5;self.scans=self.scans+1
                local found,reason,diagnostic=Ethan.discover(self.sdk,self.current_epoch)
                self.diagnostic=diagnostic
                if not found then self.state=reason;return false end
                names=found.parts;self.identity=found.identity;self.family=found.family
            else self.family='ethan' end
            for _,name in ipairs(names) do
                local backend=Ethan.new(self.sdk,name)
                if self.mode then backend.validate_show=function() return self:valid_group() end end
                self.parts[#self.parts+1]={name=name,session=HeadSession.new(backend)}
            end
        end
        local ok=self:hide_all()
        if ok then self.state=self.fault or 'HIDDEN' end
        return ok and not self.fault
    end)
end
function Group:after_apply(position)
    return self:run(function()
        if not self.enabled or not self.current_epoch or #self.parts==0 or self.fault then return false end
        if self.mode and not self:valid_group() then self:hide_all();self.state=self.fault;return false end
        local reason
        for _,part in ipairs(self.parts) do
            local s=part.session
            if not s:after_apply(position) or s.state~='VISIBLE' then
                reason=s.state
                if s.fault or (s.controller and s.controller.fault) then self.fault=reason end
                break
            end
        end
        if reason then self:hide_all();self.state=self.fault or reason;return false end
        self.state='VISIBLE';return true
    end)
end
function Group:status(detail)
    local parts,search={}, {full_scans=self.scans,cached_reads=0}
    local binding=self.fault or (self.family and ('MATCHED_'..self.family:upper()..'_RT') or self.state)
    local diagnostic_kind=self.diagnostic and self.diagnostic.kind or nil
    for _,part in ipairs(self.parts) do
        local s=part.session:status(detail)
        parts[part.name]={state=s.state,binding=s.binding,materials=s.materials,draw=s.draw,diagnostic=s.diagnostic}
        if not self.fault and s.binding:sub(1,8)~='MATCHED_' then binding=s.binding end
        diagnostic_kind=diagnostic_kind or s.diagnostic_kind
        search.full_scans=search.full_scans+s.search.full_scans
        search.cached_reads=search.cached_reads+s.search.cached_reads
    end
    return {enabled=self.enabled,state=self.state,binding=binding,
        diagnostic_kind=diagnostic_kind,diagnostic=detail and self.diagnostic or nil,
        epoch=self.epoch,family=self.family,parts=parts,search=search}
end
return Group
