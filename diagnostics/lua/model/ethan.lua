-- Experimental per-part bindings for the supplied RT model packages.
-- Cache scalar routes for reads only; reacquire/validate objects on every call.
-- Every write still scans the complete bounded tree to reject new duplicates.
local Ethan={}
Ethan.__index=Ethan
local MAX_ETHAN_INSTANCES=4
local PROFILES={
    chris={family='chris',path='ch8/character/player/pl1000/pl1002/pl1002.mesh',materials={'em7350_face__Character_Default'}},
    joe={family='joe',path='ch9/character/player/pl9000/pl9002/pl9002.mesh',materials={'pl9050_face','pl9050_hair'}},
    ethan={family='ethan',path='character/player/pl0000/pl0002/pl0002.mesh',materials={'pl0050_face','pl0050_eye','pl0050_hair','pl0050_eye_tear','pl0050_eye_shadow'}},
    mia2000={family='mia2000',path='character/player/pl2000/pl2002/pl2002.mesh',materials={'em2050_Skin_Evil','em2070_Hair2','em2050_Hair','em2070_Hair3','eye_Tear','em2050_Skin_Mia','em2050_Skin_Blend','em2050_Eye_Blend','Eye_Shadow'}},
    mia2100_hair={family='mia2100',path='character/player/pl2100/pl2100/pl2120.mesh',materials={'em2070_Hair2','em2070_Hair3'}},
    mia2100={family='mia2100',path='character/player/pl2100/pl2102/pl2102.mesh',materials={'em2050_Skin_Evil','em2050_Hair','eye_Tear','em2050_Skin_Mia','em2050_Skin_Blend','em2050_Eye_Blend','Eye_Shadow'}},
    clancy3000={family='clancy3000',path='character/player/pl3000/pl3002/pl3002.mesh',materials={'pl3050_face__Character_Default','pl3050_eye__Character_Default','pl3050_Equipment__Character_Default','pl3050_hair__Character_AlphaTest'}},
    clancy3100={family='clancy3100',path='character/player/pl3100/pl3102/pl3102.mesh',materials={'pl3050_face__Character_Default','pl3050_eye__Character_Default','pl3050_Equipment__Character_Default','pl3050_hair__Character_AlphaTest'}},
    zoe={family='zoe',path='character/player/pl3400/pl3402/pl3402.mesh',materials={'em3450_eye','em3450_hair','em3450_Eye_Shadow','em3450_Eye_Tier','pl3450_face','pl3400_hair'}},
}
local BY_PATH={}
for name,p in pairs(PROFILES) do p.name=name;p.set={};for _,m in ipairs(p.materials) do p.set[m]=true end;BY_PATH[p.path]=p end
local function reject(reason,diagnostic) error({reason=reason,diagnostic=diagnostic},0) end
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
    s=s:gsub('%.220128762$','')
    return BY_PATH[s] and s or nil
end
local function signature(mesh,profile)
    if call(mesh,'get_MaterialNum')~=#profile.materials then reject('WAIT_MODEL_MATERIALS') end
    local seen={}
    for i=0,#profile.materials-1 do
        local name=call(mesh,'getMaterialName',i)
        if not profile.set[name] or seen[name] then reject('WAIT_MODEL_MATERIALS') end
        seen[name]=true
    end
end
local function player_root(self)
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
    return root,address(player)..':'..address(root)
end
local function node_owner(tr,parent)
    if not is_type(tr,'via.Transform') then reject('WAIT_MODEL_TREE') end
    if parent and not same(call(tr,'get_Parent'),parent) then reject('WAIT_MODEL_ANCESTRY') end
    local go=call(tr,'get_GameObject')
    if not same(call(go,'get_Transform'),tr) then reject('WAIT_MODEL_ANCESTRY') end
    return go
end
local function array_components(go)
    local array=call(go,'get_Components')
    -- get_elements returns a Sol vector on REFramework, not necessarily a Lua
    -- table. Index the SystemArray directly and validate its bound before reads.
    if not array then
        reject('WAIT_MODEL_COMPONENTS',{kind='COMPONENT_ARRAY_UNAVAILABLE',owner=address(go)})
    end
    if type(array.get_size)~='function' or type(array.get_element)~='function' then
        reject('WAIT_MODEL_COMPONENTS',{kind='COMPONENT_ARRAY_ACCESS',owner=address(go)})
    end
    local size=array:get_size()
    if type(size)~='number' or size~=size or size<0 or size%1~=0 then
        reject('WAIT_MODEL_COMPONENTS',{kind='COMPONENT_ARRAY_SIZE',owner=address(go)})
    end
    if size>64 then reject('WAIT_MODEL_COMPONENT_LIMIT',{kind='COMPONENT_ARRAY_LIMIT',owner=address(go)}) end
    local components={}
    for i=0,size-1 do
        local component=array:get_element(i)
        if not component then
            reject('WAIT_MODEL_COMPONENTS',{kind='COMPONENT_ARRAY_HOLE',owner=address(go),index=i})
        end
        components[#components+1]=component
    end
    return components
end
local function component_list(tr,go)
    local ok,components=pcall(array_components,go)
    if ok then return components,'array' end
    -- Some RE7 get_Components results do not expose the SystemArray interface.
    -- EMV uses Component.get_Chain for this case. Validate the COMPLETE chain,
    -- including repeated types, instead of stopping at the first Mesh/type.
    if type(components)~='table' or not components.diagnostic
        or components.diagnostic.kind~='COMPONENT_ARRAY_ACCESS'
        or not pcall(method,tr,'get_Chain') then error(components,0) end
    local list,seen,current={}, {},tr
    while current~=nil do
        if not is_type(current,'via.Component') then
            reject('WAIT_MODEL_COMPONENTS',{kind='COMPONENT_CHAIN_TYPE',owner=address(go)})
        end
        local id=address(current)
        if seen[id] then
            if same(current,tr) then return list,'chain' end
            reject('WAIT_MODEL_COMPONENTS',{kind='COMPONENT_CHAIN_CYCLE',owner=address(go)})
        end
        if #list>=64 then
            reject('WAIT_MODEL_COMPONENT_LIMIT',{kind='COMPONENT_CHAIN_LIMIT',owner=address(go)})
        end
        if not same(call(current,'get_GameObject'),go) then
            reject('WAIT_MODEL_COMPONENTS',{kind='COMPONENT_CHAIN_OWNER',owner=address(go),component=id})
        end
        seen[id]=true;list[#list+1]=current
        current=call(current,'get_Chain')
    end
    return list,'chain'
end
local function optional_name(go)
    local ok,name=pcall(call,go,'get_Name')
    -- Omit long names: byte truncation can split UTF-8 and stop JSON logging.
    if ok and type(name)=='string' and #name<=80 then return name:gsub('%c','?') end
end
local function candidate(self,tr,go,root_id)
    local list,source=component_list(tr,go)
    local meshes,matching,components=0,nil,{}
    for _,component in ipairs(list) do
        if is_type(component,'via.render.Mesh') then
            meshes=meshes+1
            local resource=call(component,'getMesh')
            local profile=resource and BY_PATH[resource_path(resource)]
            components[meshes]={mesh=component,profile=profile and profile.name or 'OTHER'}
            local selected=profile and (self.discover_only and (not self.only_profile or profile==self.only_profile)
                or not self.discover_only and profile==self.profile)
            if selected then
                signature(component,profile);matching={mesh=component,resource=resource,profile=profile}
            end
        end
    end
    if not matching then return nil end
    if meshes~=1 then
        -- Evidence only: retain the existing rejection even for repeated array
        -- references. Never retain SDK objects or unbounded resource strings.
        local seen,unique,entries={},0,{}
        for _,entry in ipairs(components) do
            local id=address(entry.mesh)
            if not seen[id] then seen[id]=true;unique=unique+1 end
            if #entries<4 then entries[#entries+1]={mesh=id,profile=entry.profile} end
        end
        reject('WAIT_MODEL_AMBIGUOUS',{kind='MULTIPLE_MESH_COMPONENTS',owner=address(go),
            mesh_count=meshes,unique_mesh_count=unique,meshes=entries,truncated=meshes>#entries})
    end
    method(go,'get_DrawSelf');method(go,'set_DrawSelf')
    local joint=call(tr,'getJointByName','Head')
    if not is_type(joint,'via.Joint') or not same(call(joint,'get_Owner'),tr) then
        reject('WAIT_MODEL_HEAD_JOINT')
    end
    method(joint,'get_Position')
    -- Render ownership belongs to the live GameObject/Transform/Mesh within a
    -- native player generation. A resource holder or joint returned by a getter
    -- need not be the same wrapper on the next call. Still revalidate the exact
    -- resource path, material signature and joint owner on EVERY resolution.
    local info={owner=address(go),transform=address(tr),mesh=address(matching.mesh),root=root_id,
        resource_holder=address(matching.resource),joint=address(joint),components=source,
        name=optional_name(go)}
    return {go=go,mesh=matching.mesh,joint=joint,profile=matching.profile,root_id=root_id,info=info,
        id=self.epoch..':'..root_id..':'..info.owner..':'..info.transform..':'..info.mesh..':'..matching.profile.name}
end
local function duplicate_targets(first,second)
    local targets={}
    for _,t in ipairs({first,second}) do
        targets[#targets+1]={profile=t.profile.name,owner=address(t.go),mesh=address(t.mesh)}
    end
    -- The traversal stops at the first duplicate; these are two witnesses,
    -- not a claim that the entire hierarchy contains exactly two heads.
    return {kind='MULTIPLE_HEAD_OBJECTS',targets=targets}
end
local function render_identity(info)
    -- Four failed parts can share one binding record. Keep descriptive names
    -- and transient getter addresses in model_part, not this comparison list.
    return info and {owner=info.owner,transform=info.transform,mesh=info.mesh,root=info.root} or nil
end
local function full_resolve(self)
    local root,root_id=player_root(self)
    local visited,count,target,route,found_parts,meshes={},0,nil,{},{},{}
    local observed={}
    local function visit(tr,parent,depth)
        count=count+1
        if count>128 or depth>12 then reject('WAIT_MODEL_TREE_LIMIT') end
        local id=address(tr)
        if visited[id] then reject('WAIT_MODEL_TREE') end
        visited[id]=true
        local go=node_owner(tr,parent)
        if parent then -- Never select the player root.
            local found=candidate(self,tr,go,root_id)
            if found then
                if #observed<4 then observed[#observed+1]=render_identity(found.info) end
                local name=found.profile.name
                local instances=found_parts[name] or {}
                if #instances>0 and not (self.allow_instances and name=='ethan') then
                    reject('WAIT_MODEL_AMBIGUOUS',duplicate_targets(instances[1],found))
                end
                if #instances>=MAX_ETHAN_INSTANCES then
                    reject('WAIT_MODEL_INSTANCE_LIMIT',{kind='ETHAN_INSTANCE_LIMIT',limit=MAX_ETHAN_INSTANCES})
                end
                local mesh_id=address(found.mesh)
                if meshes[mesh_id] then
                    local d=duplicate_targets(meshes[mesh_id],found);d.kind='SHARED_HEAD_MESH'
                    reject('WAIT_MODEL_AMBIGUOUS',d)
                end
                meshes[mesh_id]=found
                instances[#instances+1]=found;found_parts[name]=instances
                found.route={};for i=1,depth do found.route[i]=route[i] end
                if not self.target_id or found.id==self.target_id then target=found end
            end
        end
        local child,index=call(tr,'get_Child'),1
        while child do
            route[depth+1]=index;visit(child,tr,depth+1)
            child=call(child,'get_Next');index=index+1
        end
    end
    visit(root,nil,0)
    if not target then
        reject(self.target_id and 'WAIT_MODEL_TARGET' or 'WAIT_MODEL_NOT_FOUND',
            self.target_id and {kind='TARGET_IDENTITY_MISMATCH',expected=render_identity(self.target_info),observed=observed} or nil)
    end
    if self.discover_only then
        local family,parts,identities,targets=nil,{},{},{}
        for name,instances in pairs(found_parts) do
            table.sort(instances,function(a,b) return a.id<b.id end)
            for i,t in ipairs(instances) do
                if family and family~=t.profile.family then reject('WAIT_MODEL_MIXED_FAMILY') end
                family=t.profile.family
                local key=i==1 and name or name..'_'..i
                parts[#parts+1]=key;identities[#identities+1]=name..':'..t.id
                targets[key]={profile=name,id=t.id,owner=address(t.go),mesh=address(t.mesh),copies=#instances,info=t.info}
            end
        end
        for name,p in pairs(PROFILES) do
            if p.family==family and not found_parts[name] then reject('WAIT_MODEL_COMPANION') end
        end
        table.sort(parts);table.sort(identities)
        return {family=family,parts=parts,targets=targets,identity=table.concat(identities,'|')}
    end
    return target
end
local function cached_resolve(self)
    local cached=self.cache
    local tr,root_id=player_root(self)
    if root_id~=cached.root_id then return nil end
    local traversed=0
    for _,index in ipairs(cached.route) do
        local parent=tr
        tr=call(parent,'get_Child')
        for i=1,index do
            traversed=traversed+1
            if not tr or traversed>128 then return nil end
            if i<index then tr=call(tr,'get_Next') end
        end
        node_owner(tr,parent)
    end
    local target=candidate(self,tr,node_owner(tr,nil),root_id)
    return target and target.id==cached.id and target or nil
end
function Ethan.new(sdk_api,profile_name,binding)
    local profile=PROFILES[profile_name or 'ethan'];assert(profile,'unknown RT profile')
    return setmetatable({sdk=sdk_api,profile=profile,note='WAIT_MODEL_NATIVE',full_scans=0,cached_reads=0,
        target_id=binding and binding.id or nil,allow_instances=binding~=nil,
        preserve_draw=binding and binding.copies>1 or false,
        target_info=binding and (binding.info or {owner=binding.owner,mesh=binding.mesh}) or nil},Ethan)
end
function Ethan.discover(sdk_api,epoch,ethan_only)
    local b=Ethan.new(sdk_api);b:set_epoch(epoch);b.discover_only=true
    b.only_profile=ethan_only and PROFILES.ethan or nil;b.allow_instances=true
    local ok,result=pcall(full_resolve,b)
    if not ok then return nil,type(result)=='table' and result.reason or 'WAIT_MODEL_API',
        type(result)=='table' and result.diagnostic or nil end
    return result
end
function Ethan:set_epoch(epoch)
    local next_epoch=type(epoch)=='string' and #epoch>0 and #epoch<=110 and epoch or nil
    if next_epoch~=self.epoch then self.cache=nil;self.diagnostic=nil end
    self.epoch=next_epoch
end
function Ethan:resolve(allow_cache)
    if allow_cache and self.cache then
        local ok,target=pcall(cached_resolve,self)
        if ok and target then
            self.cached_reads=self.cached_reads+1;self.diagnostic=nil
            self.target_info=target.info
            self.note='MATCHED_'..self.profile.name:upper()..'_RT';return target
        end
    end
    self.cache=nil;self.full_scans=self.full_scans+1
    local ok,target=pcall(full_resolve,self)
    if not ok then
        self.note=type(target)=='table' and target.reason or 'WAIT_MODEL_API'
        self.diagnostic=type(target)=='table' and target.diagnostic or nil
        return nil
    end
    -- Store ONLY scalar identities and sibling indices, never target userdata.
    self.cache={id=target.id,root_id=target.root_id,route=target.route}
    self.target_info=target.info
    self.diagnostic=nil
    self.note='MATCHED_'..self.profile.name:upper()..'_RT'
    return target
end
local function read_draw(self,allow_cache)
    local t=self:resolve(allow_cache)
    if not t then return nil end
    local value=call(t.go,'get_DrawSelf')
    if type(value)~='boolean' then self.note='WAIT_MODEL_DRAW_TYPE';return nil end
    -- Optional evidence only: DrawSelf is a request and can differ from Draw.
    -- Neither getter proves the final pixels; unsupported Draw must not fault.
    local ok,draw=pcall(call,t.go,'get_Draw')
    self.draw={draw_self=value,effective_draw=ok and type(draw)=='boolean' and draw or nil}
    if ok and draw==false then self.draw.effective_draw=false end
    return {id=t.id,visible=value}
end
function Ethan:read() return read_draw(self,false) end
-- The visibility lifecycle may use this read because every mutation below
-- independently rechecks uniqueness. Standalone read() keeps its full-scan contract.
function Ethan:read_cached() return read_draw(self,true) end
function Ethan:material_names()
    local names={};for i,name in ipairs(self.profile.materials) do names[i]=name end
    return names
end
local function material_index(target,name)
    -- Resolve the current index by name; material order can change without a
    -- resource/owner change. Signature validation still requires the exact set.
    method(target.mesh,'getMaterialsEnable');method(target.mesh,'setMaterialsEnable')
    for i=0,#target.profile.materials-1 do
        if call(target.mesh,'getMaterialName',i)==name then return i end
    end
end
function Ethan:read_material(name)
    if not self.profile.set[name] then return nil end
    local t=self:resolve(true)
    if not t then return nil end
    local index=material_index(t,name)
    if index==nil then return nil end
    local value=call(t.mesh,'getMaterialsEnable',index)
    if type(value)~='boolean' then self.note='WAIT_MODEL_MATERIAL_ENABLE_TYPE';return nil end
    return {id=t.id,visible=value}
end
function Ethan:write_material(id,name,expected,wanted)
    if not self.profile.set[name] or type(expected)~='boolean' or type(wanted)~='boolean' then return false end
    if wanted and self.validate_show and not self.validate_show() then return false end
    local t=self:resolve(false)
    if not t or t.id~=id then return false end
    local index=material_index(t,name)
    if index==nil or call(t.mesh,'getMaterialsEnable',index)~=expected then return false end
    call(t.mesh,'setMaterialsEnable',index,wanted)
    return true
end
function Ethan:write(id,expected,wanted)
    if type(expected)~='boolean' or type(wanted)~='boolean' then return false end
    if wanted and self.validate_show and not self.validate_show() then return false end
    local t=self:resolve(false)
    if not t or t.id~=id or call(t.go,'get_DrawSelf')~=expected then return false end
    call(t.go,'set_DrawSelf',wanted)
    return true
end
function Ethan:head_position(id)
    local t=self:resolve(true)
    if not t or t.id~=id then return nil end
    local p=call(t.joint,'get_Position')
    local xyz=p and {p.x,p.y,p.z} or {}
    for i=1,3 do
        local n=xyz[i]
        if type(n)~='number' or n~=n or math.abs(n)>1000000 then return nil end
    end
    return xyz
end
function Ethan:stats() return {full_scans=self.full_scans,cached_reads=self.cached_reads} end
return Ethan
