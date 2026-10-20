-- Test-only JSON serializer; production uses REFramework's json.dump_string.
local function quote(s)
    return '"'..s:gsub('[%z\1-\31\\"]',function(c)
        if c=='"' then return '\\"' end
        if c=='\\' then return '\\\\' end
        return string.format('\\u%04x',string.byte(c))
    end)..'"'
end
local function encode(value)
    local ty=type(value)
    if ty=='nil' then return 'null' end
    if ty=='boolean' then return tostring(value) end
    if ty=='number' then
        assert(value==value and math.abs(value)<math.huge)
        return string.format('%.17g',value)
    end
    if ty=='string' then return quote(value) end
    assert(ty=='table')
    local parts={}
    if #value>0 then
        for i=1,#value do parts[#parts+1]=encode(value[i]) end
        return '['..table.concat(parts,',')..']'
    end
    local keys={};for k in pairs(value) do keys[#keys+1]=k end;table.sort(keys)
    for _,k in ipairs(keys) do parts[#parts+1]=quote(k)..':'..encode(value[k]) end
    return '{'..table.concat(parts,',')..'}'
end
return encode
