-- RE7 OTS State Observer 0.2.1. No setters, raw writes, or hook installation.
-- SDK observations are separate from CT candidates; never claim equivalence.
local VERSION = "0.2.1"
local NATIVE_VERSION = "0.2.0" -- Lua-only camera fix; existing DLL stays compatible.
local DIR = "RE7_OTS_Observer/"
local MAX_SAMPLES, MAX_BYTES, MAX_SECONDS = 9000, 16 * 1024 * 1024, 900
local native, last_token, seen_session, changed_at, fresh = nil, nil, nil, 0, false
local last_poll, last_camera, last_seq = -math.huge, -math.huge, nil
local callback_error, last_callback_seen = nil, nil
local camera_state, io_state = "WAITING_FOR_NATIVE", "IDLE"
local file, file_session, file_bytes, file_count, file_started = nil, nil, 0, 0, 0
local finished, opened = {}, 0
local identity, generation = nil, 0
local clock = os.clock or os.time
local function now() return clock() end
local function info(message)
    if log and log.info then log.info("[RE7_OBSERVER] " .. message) end
end
local function finite(v, limit)
    return type(v) == "number" and v == v and v ~= math.huge and v ~= -math.huge and math.abs(v) <= limit
end
local function valid_status(s)
    return type(s) == "table" and s.schema == 1 and s.version == NATIVE_VERSION
        and type(s.session) == "string" and #s.session <= 95
        and s.session:match("^re7_observer_%d+_%d+_%d+_%d+_%d+_%d+_%d+_%d+$") ~= nil
        and finite(s.seq, MAX_SAMPLES) and s.seq >= 0 and s.seq % 1 == 0
        and finite(s.tick_ms, 9007199254740991) and s.tick_ms >= 0
        and type(s.stage) == "string" and #s.stage <= 24
        and type(s.phase) == "string"
end
local function close_file(reason)
    if file then
        local f = file
        file = nil
        -- EOF marker is best effort; sample writes/flush failures are surfaced.
        local ok, result = pcall(function()
            local line = json.dump_string({kind="end", reason=reason, samples=file_count}, -1) .. "\n"
            if file_bytes + #line > MAX_BYTES then return nil end
            if not f:write(line) or not f:flush() then return nil end
            return f:close()
        end)
        if not ok or not result then
            pcall(function() f:close() end)
            io_state = "ERROR_CLOSE_OR_WRITE"
        elseif io_state == "RECORDING" then io_state = reason end
    end
    if file_session then finished[file_session] = true end
    file_session = nil
    identity = nil
end
local function write_record(record)
    if not file then return false end
    local ok, result = pcall(function()
        local line = json.dump_string(record, -1) .. "\n"
        if #line > 4096 or file_bytes + #line > MAX_BYTES - 1024 then return "LIMIT" end
        if not file:write(line) or not file:flush() then return "ERROR_WRITE" end
        file_bytes = file_bytes + #line
        return "OK"
    end)
    if not ok or result ~= "OK" then
        io_state = ok and result or "ERROR_WRITE"
        close_file(io_state)
        return false
    end
    return true
end
local function open_file(s, t)
    if finished[s.session] or opened >= 4 then return false end
    opened = opened + 1
    file_session = s.session
    file_bytes, file_count, file_started, last_seq, identity, generation = 0, 0, t, nil, nil, 0
    if not io or not io.open then io_state="IO_UNAVAILABLE"; close_file(io_state); return false end
    local path = DIR .. s.session .. "_camera.jsonl"
    -- Never truncate a camera trace after script reset or a repeated session.
    local ok, existing = pcall(io.open, path, "rb")
    if not ok then io_state="ERROR_OPEN";close_file(io_state);return false end
    if existing then existing:close();io_state="EXISTING_FILE_PRESERVED";close_file(io_state);return false end
    ok, file = pcall(io.open, path, "wb")
    if not ok or not file then file=nil;io_state="ERROR_OPEN";close_file(io_state);return false end
    io_state="RECORDING"
    return write_record({kind="header",schema=1,version=VERSION,native_version=s.version,session=s.session,
        source="REFramework primary camera transform",callback="post UpdateBehavior",
        getter_path="primary_camera -> get_GameObject -> get_Transform",
        camera_ct_equivalence="NOT_TESTED",semantic_validation="NOT_PERFORMED",
        native_sample_pairing="latest observed status; not simultaneous",game_setters=false,hooks=false})
end
local function poll()
    local t = now()
    -- Enforce lifetime even when native status is absent, stale, or malformed.
    if file and (t-file_started >= MAX_SECONDS or file_count >= MAX_SAMPLES) then close_file("LIMIT") end
    if t - last_poll < 0.1 then return end
    last_poll = t
    local ok, s = pcall(json.load_file, DIR .. "status.json")
    if not ok or not valid_status(s) then
        fresh = false;native=nil;camera_state="UNKNOWN_NATIVE_STATUS"
        return
    end
    native=s
    local token=s.session .. ":" .. tostring(s.tick_ms) .. ":" .. tostring(s.seq) .. ":" .. s.phase
    if seen_session ~= s.session then
        if file then close_file("NATIVE_SESSION_CHANGED") end
        seen_session=s.session;last_token=token;changed_at=t;fresh=false
    elseif token ~= last_token then
        last_token=token;changed_at=t;fresh=true
    elseif t - changed_at > 2 then
        fresh=false;camera_state="UNKNOWN_STALE_NATIVE";identity=nil
    end
    if s.phase ~= "RECORDING" then
        if file then close_file("NATIVE_" .. s.phase) end
        camera_state="NATIVE_" .. s.phase
    elseif not fresh then camera_state="WAITING_FOR_HEARTBEAT" end
end
local function has_method(obj, name)
    local ty=obj:get_type_definition()
    for _=1,16 do
        if not ty then return false end
        if ty:get_method(name) then return true end
        ty=ty:get_parent_type()
    end
    return false
end
local function camera_snapshot()
    if not sdk or type(sdk.get_primary_camera) ~= "function" then return {status="UNKNOWN_CAMERA_API"} end
    local cam=sdk.get_primary_camera()
    if not cam then return {status="UNKNOWN_NO_CAMERA"} end
    -- via.Camera is a Component; the owning GameObject supplies its Transform.
    if not has_method(cam,"get_GameObject") then return {status="UNKNOWN_GAMEOBJECT_METHOD"} end
    local owner=cam:call("get_GameObject")
    if not owner then return {status="UNKNOWN_NO_GAMEOBJECT"} end
    if not has_method(owner,"get_Transform") then return {status="UNKNOWN_TRANSFORM_METHOD"} end
    local transform=owner:call("get_Transform")
    if not transform then return {status="UNKNOWN_NO_TRANSFORM"} end
    local ty=transform:get_type_definition()
    if not ty or not ty:is_a("via.Transform") then return {status="UNKNOWN_TRANSFORM_TYPE"} end
    local p=transform:get_position()
    local q=transform:get_rotation()
    if not p or not q then return {status="UNKNOWN_TRANSFORM_VALUE"} end
    local xyz={p.x,p.y,p.z}
    local quat={q.x,q.y,q.z,q.w}
    for i=1,3 do if not finite(xyz[i],1000000) then return {status="UNKNOWN_POSITION_VALUE"} end end
    local norm=0
    for i=1,4 do
        if not finite(quat[i],2) then return {status="UNKNOWN_ROTATION_VALUE"} end
        norm=norm+quat[i]*quat[i]
    end
    if norm<0.5 or norm>1.5 then return {status="UNKNOWN_ROTATION_NORM"} end
    local current=sdk.get_primary_camera()
    if not current or current:get_address() ~= cam:get_address() then return {status="UNKNOWN_CAMERA_CHANGED"} end
    return {status="READABLE_CANDIDATE",xyz=xyz,rotation_xyzw=quat,
        camera_id=tostring(cam:get_address()),transform_id=tostring(transform:get_address())}
end
local function observe_camera()
    local t=now()
    last_callback_seen=t
    if t-last_camera<0.1 then return end
    last_camera=t
    local s=native
    if not s or not fresh or t-changed_at>2 or s.phase~="RECORDING" then identity=nil;return end
    if file_session ~= s.session and not open_file(s,t) then return end
    if not file or s.seq==last_seq then return end
    if file_count>=MAX_SAMPLES or t-file_started>=MAX_SECONDS then close_file("LIMIT");return end
    last_seq=s.seq
    local ok, result=pcall(camera_snapshot)
    if not ok then result={status="UNKNOWN_SDK_EXCEPTION"} end
    camera_state=result.status
    if result.status=="READABLE_CANDIDATE" then
        local id=result.camera_id .. ":" .. result.transform_id
        if identity~=id then generation=generation+1 end
        identity=id
    else identity=nil end
    result.generation=generation
    file_count=file_count+1
    write_record({kind="sample",schema=1,session=s.session,seq=file_count,
        native_seq=s.seq,native_tick_ms=s.tick_ms,stage=s.stage,
        lua_clock_s=t,native_status_age_s=t-changed_at,camera=result})
end
local function draw_ui()
    if not imgui.tree_node("RE7 OTS State Observer v" .. VERSION) then return end
    imgui.text("F8: start / stop recording   |   F7: next test stage")
    imgui.text("Stages: STILL > MOVE > LOOK > AIM > INVENTORY > EXAMINE > RELOAD > AFTER_RELOAD")
    local native_phase=native and native.phase or "WAITING - press F8"
    local current=fresh and now()-changed_at<=2
    if native_phase=="RECORDING" and not current then native_phase="UNKNOWN / stale or waiting for heartbeat" end
    local camera_display=callback_error or camera_state
    if not callback_error and native and native.phase=="RECORDING" and current
        and (not last_callback_seen or now()-last_callback_seen>2) then camera_display="UNKNOWN_CALLBACK_NOT_RUNNING" end
    local file_display=io_state
    if file and not current then file_display="PAUSED_NATIVE_UNKNOWN" end
    imgui.text("Native: " .. native_phase)
    imgui.text("Camera: " .. camera_display .. " | File: " .. file_display)
    if native then
        imgui.text("Session: " .. native.session)
        imgui.text("Stage: " .. native.stage .. " | Native samples: " .. tostring(native.seq))
        for _,name in ipairs({"player","aim","inventory","interact"}) do
            if type(native[name])=="table" then imgui.text(name .. ": " .. (current and tostring(native[name].status) or "UNKNOWN_STALE_VALUE")) end
        end
    end
    imgui.text("READABLE_CANDIDATE means readable only; gameplay meaning is unverified.")
    imgui.text("No third-person patch. Logs: reframework/data/RE7_OTS_Observer")
    imgui.tree_pop()
end
if re and re.on_frame then re.on_frame(poll) end
if re and re.on_application_entry then
    local ok, result=pcall(re.on_application_entry,"UpdateBehavior",observe_camera)
    if not ok or result==false then callback_error="UNKNOWN_CALLBACK_REGISTRATION" end
else callback_error="UNKNOWN_CALLBACK_API" end
if re and re.on_draw_ui then re.on_draw_ui(draw_ui) end
if re and re.on_script_reset then re.on_script_reset(function() close_file("SCRIPT_RESET") end) end
info("v" .. VERSION .. " camera adapter loaded. F8 starts native observation.")
