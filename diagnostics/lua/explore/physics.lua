-- Experimental center ray. Only scalar evidence survives this callback.
-- RE7's empirical EMV layer set is not a verified world-only collision mask.
local Physics={}
Physics.__index=Physics
local function reject(reason) error({reason=reason},0) end
local function call(obj,name,...)
    if not obj then reject('OBJECT') end
    local ty=obj:get_type_definition()
    for _=1,16 do
        if not ty then break end
        if ty:get_method(name) then return obj:call(name,...) end
        ty=ty:get_parent_type()
    end
    reject('METHOD:'..name)
end
local function address(obj)
    local n=obj and obj:get_address()
    if not Core.finite(n,9007199254740991) or n<=0 or n%1~=0 then reject('ADDRESS') end
    return n
end
local function is_player(collider,root_id)
    local go=call(collider,'get_GameObject')
    if not go then return false end -- An ownerless contact still obstructs the ray.
    local tr=call(go,'get_Transform')
    if not tr then reject('TRANSFORM') end
    local visited={}
    for _=1,32 do
        local id=address(tr)
        if id==root_id then return true end
        if visited[id] then reject('ANCESTRY') end
        visited[id]=true
        tr=call(tr,'get_Parent')
        if not tr then return false end
    end
    reject('ANCESTRY_LIMIT')
end
function Physics.new(sdk_api) return setmetatable({sdk=sdk_api},Physics) end
function Physics:cast(origin,target)
    local evidence={status='unavailable',casts=0,contacts=0}
    local result
    local ok,reason=pcall(function()
        if not Core.vector(origin,3,1000000) or not Core.vector(target,3,1000000) then reject('ENDPOINT') end
        local length=Core.distance(origin,target)
        if length<0.000001 then reject('LENGTH') end
        local direction={}
        for i=1,3 do direction[i]=(target[i]-origin[i])/length end
        local sdk=self.sdk
        local manager=sdk.get_managed_singleton('app.ObjectManager')
        local player=manager and manager:get_field('PlayerObj')
        local root_id=address(call(player,'get_Transform'))
        local system=sdk.get_native_singleton('via.physics.System')
        local ty=sdk.find_type_definition('via.physics.System')
        local cast=ty and ty:get_method('castRay(via.physics.CastRayQuery, via.physics.CastRayResult)')
        local point_type=sdk.find_type_definition('via.physics.ContactPoint')
        if not system or not cast or not point_type then reject('API') end
        local query=sdk.create_instance('via.physics.CastRayQuery'):add_ref()
        result=sdk.create_instance('via.physics.CastRayResult'):add_ref()
        call(query,'clearOptions');call(query,'enableAllHits');call(query,'enableNearSort')
        local filter=call(query,'get_FilterInfo')
        call(filter,'set_Group',0);call(filter,'set_MaskBits',2)
        call(query,'setRay(via.vec3, via.vec3)',Vector3f.new(origin[1],origin[2],origin[3]),
            Vector3f.new(target[1],target[2],target[3]))
        call(result,'clear')
        for layer=0,32 do
            if layer~=7 and layer~=18 then
                call(filter,'set_Layer',layer);call(query,'set_FilterInfo',filter)
                evidence.casts=evidence.casts+1
                cast:call(system,query,result) -- Reference API does not require a boolean return.
                local count=call(result,'get_NumContactPoints')
                if not Core.finite(count,64) or count<0 or count%1~=0 then reject('CONTACT_LIMIT') end
                evidence.contacts=evidence.contacts+count
                if evidence.contacts>256 then reject('CONTACT_TOTAL_LIMIT') end
                for i=0,count-1 do
                    local point=call(result,'getContactPoint(System.UInt32)',i)
                    local collider=call(result,'getContactCollidable(System.UInt32)',i)
                    local position=point and sdk.get_native_field(point,point_type,'Position')
                    local p=position and {position.x,position.y,position.z}
                    if not Core.vector(p,3,1000000) then reject('CONTACT_POSITION') end
                    local delta,projection={},0
                    for j=1,3 do delta[j]=p[j]-origin[j];projection=projection+delta[j]*direction[j] end
                    local perpendicular=0
                    for j=1,3 do perpendicular=perpendicular+(delta[j]-projection*direction[j])^2 end
                    if projection < -0.02 or projection>length+0.02 or perpendicular>0.02^2 then reject('CONTACT_GEOMETRY') end
                    if not is_player(collider,root_id) then
                        local distance=math.max(0,math.min(length,projection))
                        if not evidence.distance or distance<evidence.distance then
                            evidence.distance=distance;evidence.hit=p;evidence.layer=layer
                        end
                    end
                end
                call(result,'clear')
            end
        end
        evidence.status=evidence.distance and 'hit' or 'clear'
    end)
    if not ok then
        -- Clear only this newly owned result; normal Lua ref tracking releases it.
        if result then pcall(call,result,'clear') end
        evidence.status='unavailable';evidence.distance=nil;evidence.hit=nil;evidence.layer=nil
        evidence.reason=type(reason)=='table' and reason.reason or 'SDK_ERROR'
    end
    return evidence
end
return Physics
