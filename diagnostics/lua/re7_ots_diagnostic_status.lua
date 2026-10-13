-- Optional status display only. No game getters/setters, hooks or scan commands.
-- Install alongside the native DLL; a saved status may predate this launch.
local status = nil
local last_poll = -1
local function poll()
    local ok, value = pcall(json.load_file, "RE7_OTS_Diagnostics/status.json")
    if ok and type(value) == "table" and type(value.phase) == "string" then
        status = value
    end
end
re.on_draw_ui(function()
    if not imgui.tree_node("RE7 OTS Diagnostics (read-only)") then return end
    local now = os.time()
    if now ~= last_poll then
        last_poll = now
        poll()
    end
    imgui.text("This tool diagnoses bytes. It does NOT enable third person.")
    if status then
        imgui.text("Latest reported status: " .. status.phase)
        imgui.text("Report: " .. tostring(status.report or "unknown"))
        if status.phase == "COMPLETE" then
            imgui.text("Scan finished. Share the named .txt report, not re7.exe.")
        elseif string.sub(status.phase, 1, 5) == "ERROR" then
            imgui.text("Diagnostic error. Check the report and REFramework log.")
        else
            imgui.text("Diagnostic is running. No game patch is being applied.")
        end
    else
        imgui.text("No status yet. Check that RE7_OTS_Diagnostics.dll loaded.")
    end
    imgui.text("Saved status can be from an earlier launch. Check the filename time.")
    imgui.text("F9: request a new snapshot while idle. No F9 is needed for first scan.")
    imgui.text("Reports: reframework/data/RE7_OTS_Diagnostics/")
    if imgui.button("Refresh diagnostic status") then poll() end
    imgui.tree_pop()
end)
