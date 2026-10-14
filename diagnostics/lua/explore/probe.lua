-- Read-only, bounded scalar observations. No retained SDK objects or setters.
local Probe={}
local function xyz(v)
    if not v then return nil end
    local p={v.x,v.y,v.z}
    return Core.vector(p,3,1000000) and p or nil
end
local function capture(fn)
    local ok,r=pcall(fn)
    return ok and r or {status='UNAVAILABLE'}
end
function Probe.camera(c)
    return capture(function()
        if not c then return {status='NO_CAMERA'} end
        local r={id=c.id,status='OK',transform=xyz(c.tr:get_position())}
        local q=c.tr:get_rotation()
        local values=q and {q.x,q.y,q.z,q.w}
        if Core.vector(values,4,2) then r.rotation=values end
        r.joint=capture(function()
            local j=c.tr:call('getJointByName','Camera')
            if not j then return {status='NO_CAMERA_JOINT'} end
            return {status='OK',position=xyz(j:call('get_Position'))}
        end)
        return r
    end)
end
local function component(sdk_api,player,name)
    local t=sdk_api.typeof(name)
    if not t then return nil end
    local c=player:call('getComponent(System.Type)',t)
    local owner=c and c:call('get_GameObject')
    if not owner or owner:get_address()~=player:get_address() then return nil end
    return c
end
function Probe.player(sdk_api)
    return capture(function()
        local manager=sdk_api.get_managed_singleton('app.ObjectManager')
        local player=manager and manager:get_field('PlayerObj')
        if not player then return {status='NO_PLAYER'} end
        local r={status='OK'}
        r.hands=capture(function()
            local hand=component(sdk_api,player,'app.PlayerHandTouch')
            if not hand then return {status='NO_HAND_TOUCH'} end
            local array=hand:get_field('HandIK')
            local size=array and array:get_size()
            if not Core.finite(size,8) or size<0 or size%1~=0 then return {status='INVALID_HAND_ARRAY'} end
            local targets={}
            for i=0,math.min(size,2)-1 do
                local ik=array:get_element(i)
                local tr=ik and ik:get_field('Target')
                local ty=tr and tr:get_type_definition()
                local p=ty and ty:is_a('via.Transform') and xyz(tr:get_position())
                targets[i+1]=p or {status='NO_TARGET'}
            end
            return {status='OK',count=size,targets=targets}
        end)
        r.motion=capture(function()
            local motion=component(sdk_api,player,'app.PlayerMotionController')
            if not motion then return {status='NO_MOTION_CONTROLLER'} end
            local values={status='OK'}
            for _,name in ipairs({'IsRArmJacked','IsPosturalCameraControl'}) do
                local v=motion:get_field(name)
                if type(v)=='boolean' then values[name]=v end
            end
            return values
        end)
        return r
    end)
end
return Probe
