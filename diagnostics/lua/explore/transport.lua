-- Select a transport once. A failing direct API must never consume an old file.
local Transport={}
Transport.__index=Transport
function Transport.new(api,json_api,dir)
    local valid=type(api)=='table' and api.api_version==1 and type(api.read)=='function'
        and type(api.clock)=='function' and type(json_api.load_string)=='function'
    local mode=api==nil and 'LEGACY_FILE' or valid and 'NATIVE_DIRECT' or 'NATIVE_INCOMPATIBLE'
    return setmetatable({api=api,json=json_api,dir=dir,mode=mode,last_time=0,
        legacy_clock=os.clock or os.time,
        evidence={mode=mode,status='NONE',attempts=0,recovered_total=0,failed_total=0}},Transport)
end
function Transport:now()
    if self.mode~='NATIVE_DIRECT' then return self.legacy_clock() end
    local ok,t=pcall(self.api.clock)
    if not ok or type(t)~='number' or t~=t or t==math.huge or t<self.last_time then
        self.clock_failed=true;return self.last_time
    end
    self.last_time=t;return t
end
function Transport:read()
    local old=self.evidence
    local r={mode=self.mode,attempts=0,recovered_total=old.recovered_total,failed_total=old.failed_total}
    local value
    if self.mode=='NATIVE_INCOMPATIBLE' then r.status='INCOMPATIBLE_API'
    elseif self.clock_failed then r.status='CLOCK_ERROR'
    else
        for attempt=1,self.mode=='LEGACY_FILE' and 2 or 1 do
            local ok,s=pcall(function()
                if self.mode=='LEGACY_FILE' then return self.json.load_file(self.dir..'status.json') end
                local raw=self.api.read()
                if raw==nil then return nil end
                if type(raw)~='string' or #raw>8192 then error('invalid native payload') end
                return self.json.load_string(raw)
            end)
            r.attempts=attempt
            r.status=not ok and 'READ_ERROR' or s==nil and 'EMPTY' or type(s)=='table' and 'DECODED' or 'INVALID_TYPE'
            if ok and s~=nil then value=s;break end
            if attempt==1 then r.first_failure=r.status end
        end
    end
    if value==nil then r.failed_total=r.failed_total+1
    elseif r.attempts==2 then r.recovered_total=r.recovered_total+1 end
    self.evidence=r;return value
end
function Transport:status() return self.evidence end
return Transport
