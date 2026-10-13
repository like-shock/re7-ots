-- A bounded two-file play journal; failures never control the camera lifecycle.
local M={}
function M.new(io_api,json_api,dir,header,max_bytes)
    local self={state='IDLE',file=nil,bytes=0,slot=0,records=0,failed=false}
    local cap=max_bytes or 2*1024*1024
    local function fail(reason)
        self.state=reason;self.failed=true
        if self.file then pcall(function() self.file:close() end);self.file=nil end
        return false
    end
    local function encode(r)
        local ok,s=pcall(json_api.dump_string,r,-1)
        if not ok or type(s)~='string' then fail('ERROR_JSON');return nil end
        if #s+1>4096 or #s+1>cap-256 then fail('ERROR_LINE_SIZE');return nil end
        return s..'\n'
    end
    local function write(line)
        if self.bytes+#line>cap then return fail('ERROR_LIMIT') end
        local ok,result=pcall(function() return self.file:write(line) and self.file:flush() end)
        if not ok or not result then return fail('ERROR_WRITE') end
        self.bytes=self.bytes+#line;return true
    end
    function self:close(reason)
        if not self.file then return end
        local line=encode({kind='end',reason=reason,records=self.records})
        if not line or not self.file then return end
        if not write(line) then return end
        local f=self.file;self.file=nil
        local ok,result=pcall(function() return f:close() end)
        if not ok or not result then fail('ERROR_CLOSE') else self.state=reason end
    end
    local function open()
        if not io_api or not io_api.open then return fail('ERROR_IO_API') end
        local ok,f=pcall(io_api.open,dir..'play_'..self.slot..'.jsonl','wb')
        if not ok or not f then return fail('ERROR_OPEN') end
        self.file=f;self.bytes=0;self.records=0;self.state='RECORDING'
        local line=encode(header);return line and write(line) or false
    end
    function self:record(record)
        if self.failed then return false end
        local line=encode(record);if not line then return false end
        if not self.file and not open() then return false end
        if self.bytes+#line>cap-256 then
            self:close('ROTATE');if self.failed then return false end
            self.slot=1-self.slot
            if not open() then return false end
        end
        if not write(line) then return false end
        self.records=self.records+1;return true
    end
    return self
end
return M
