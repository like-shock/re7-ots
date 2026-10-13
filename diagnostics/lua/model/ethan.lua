-- Experimental binding for the supplied Ethan with Head RT package.
-- No managed object survives a call. Re-scan the bounded local-player subtree
-- on EVERY resolution: a cached route alone cannot detect newly added duplicates.
local Ethan={}
Ethan.__index=Ethan
local PATH='character/player/pl0000/pl0002/pl0002.mesh'
local MATERIALS={pl0050_face=true,pl0050_eye=true,pl0050_hair=true,
    pl0050_eye_tear=true,pl0050_eye_shadow=true}
local function reject(reason) error({reason=reason},0) end
local function method(o,name)
    if not o then reject('WAIT_MODEL_OBJECT') end
    local ty=o:get_type_definition()
    for _=1,16 do
        if not ty then break end
        if ty:get_method(name) then return end
        ty=ty:get_parent_type()
    end
    reject('WAIT_MODEL_METHOD:'..name)
end
local function call(o,name,...)
    method(o,name);return o:call(name,...)
end
local function address(o)
    if not o then reject('WAIT_MODEL_OBJECT') end
    local n=o:get_address()
    if type(n)~='number' or n~=n or n<=0 or n>9007199254740991 or n%1~=0 then
        reject('WAIT_MODEL_ADDRESS')
    end
    return tostring(n)
end
local function same(a,b) return a and b and address(a)==address(b) end
local function is_type(o,name)
    local ty=o and o:get_type_definition()
    return ty and ty:is_a(name)
end
local function resource_path(resource)
    local s=call(resource,'ToString()')
    if type(s)~='string' or #s>1024 then return nil end
    -- EMV's resource representation is Type[@path] (some builds omit @).
    s=s:match('^[^%[%]]+%[@?([^%[%]]+)%]$')
    if not s then return nil end
    s=s:lower():gsub('\\','/'):gsub('^natives/stm/','')
    if s==PATH or s==PATH..'.220128762' then return PATH end
    return nil
end
local function signature(mesh)
    if call(mesh,'get_MaterialNum')~=5 then reject('WAIT_MODEL_MATERIALS') end
    local seen={}
    for i=0,4 do
        local name=call(mesh,'getMaterialName',i)
        if not MATERIALS[name] or seen[name] then reject('WAIT_MODEL_MATERIALS') end
        seen[name]=true
    end
end
local function resolve(self)
    if not self.epoch then reject('WAIT_MODEL_NATIVE') end
    if not self.sdk or type(self.sdk.get_managed_singleton)~='function' then reject('WAIT_MODEL_SDK') end
    local manager=self.sdk.get_managed_singleton('app.ObjectManager')
    if not manager then reject('WAIT_MODEL_PLAYER') end
    local player=manager:get_field('PlayerObj')
    if not player then reject('WAIT_MODEL_PLAYER') end
    local root=call(player,'get_Transform')
    if not is_type(root,'via.Transform') or not same(call(root,'get_GameObject'),player) then
        reject('WAIT_MODEL_ROOT')
    end
    local root_id=address(player)..':'..address(root)
    local visited,count,target={},0,nil
    local function visit(tr,parent,depth)
        count=count+1
        if count>128 or depth>12 then reject('WAIT_MODEL_TREE_LIMIT') end
        local id=address(tr)
        if visited[id] or not is_type(tr,'via.Transform') then reject('WAIT_MODEL_TREE') end
        visited[id]=true
        if parent and not same(call(tr,'get_Parent'),parent) then reject('WAIT_MODEL_ANCESTRY') end
        local go=call(tr,'get_GameObject')
        if not same(call(go,'get_Transform'),tr) then reject('WAIT_MODEL_ANCESTRY') end
        -- Never select the local-player root, even if its mesh happens to match.
        if parent then
            local array=call(go,'get_Components')
            local elements=array and array:get_elements()
            if type(elements)~='table' then reject('WAIT_MODEL_COMPONENTS') end
            local meshes,matching,component_count=0,nil,0
            for _,component in pairs(elements) do
                component_count=component_count+1
                if component_count>64 then reject('WAIT_MODEL_COMPONENT_LIMIT') end
                if is_type(component,'via.render.Mesh') then
                    meshes=meshes+1
                    local resource=call(component,'getMesh')
                    if resource and resource_path(resource)==PATH then
                        signature(component);matching={mesh=component,resource=resource}
                    end
                end
            end
            if matching then
                if meshes~=1 or target then reject('WAIT_MODEL_AMBIGUOUS') end
                method(go,'get_DrawSelf');method(go,'set_DrawSelf')
                local joint=call(tr,'getJointByName','Head')
                if not is_type(joint,'via.Joint') or not same(call(joint,'get_Owner'),tr) then
                    reject('WAIT_MODEL_HEAD_JOINT')
                end
                method(joint,'get_Position')
                target={go=go,joint=joint,id=self.epoch..':'..root_id..':'..address(go)..':'..id
                    ..':'..address(matching.mesh)..':'..address(matching.resource)..':'..address(joint)}
            end
        end
        local child=call(tr,'get_Child')
        while child do
            visit(child,tr,depth+1)
            child=call(child,'get_Next')
        end
    end
    visit(root,nil,0)
    if not target then reject('WAIT_MODEL_NOT_FOUND') end
    return target
end
function Ethan.new(sdk_api)
    return setmetatable({sdk=sdk_api,note='WAIT_MODEL_NATIVE'},Ethan)
end
function Ethan:set_epoch(epoch)
    self.epoch=type(epoch)=='string' and #epoch>0 and #epoch<=110 and epoch or nil
end
function Ethan:resolve()
    local ok,target=pcall(resolve,self)
    if not ok then
        self.note=type(target)=='table' and target.reason or 'WAIT_MODEL_API'
        return nil
    end
    self.note='MATCHED_ETHAN_RT'
    return target
end
function Ethan:read()
    local t=self:resolve()
    if not t then return nil end
    local value=call(t.go,'get_DrawSelf')
    if type(value)~='boolean' then self.note='WAIT_MODEL_DRAW_TYPE';return nil end
    return {id=t.id,visible=value}
end
function Ethan:write(id,expected,wanted)
    if type(expected)~='boolean' or type(wanted)~='boolean' then return false end
    local t=self:resolve()
    if not t or t.id~=id or call(t.go,'get_DrawSelf')~=expected then return false end
    -- A thrown setter can mutate: let Visibility perform its immediate readback.
    call(t.go,'set_DrawSelf',wanted)
    return true
end
function Ethan:head_position(id)
    local t=self:resolve()
    if not t or t.id~=id then return nil end
    local p=call(t.joint,'get_Position')
    local xyz=p and {p.x,p.y,p.z} or {}
    for i=1,3 do
        local n=xyz[i]
        if type(n)~='number' or n~=n or math.abs(n)>1000000 then return nil end
    end
    return xyz
end
return Ethan
