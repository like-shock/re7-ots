# RE7 OTS read-only diagnostic, v0.1.0

The user approved local diagnosis instead of uploading re7.exe. Target: SteamOS / Proton, public RT RE7, working REFramework and RELit. Deliverable is a Windows x64 native REFramework diagnostic DLL, not a third-person mod.

The DLL reads only the main executable and the process's own readable mapped image; it never installs CT patches, calls game setters, writes executable pages, enables CE, uses a driver, or sends network traffic. Its only persistent writes are text reports and status JSON in reframework/data/RE7_OTS_Diagnostics. It validates the selected 78 CT AOB sites, complete overwrite byte sequences, and fixed-RVA accessibility. It does not establish object semantics or playable mod compatibility.

Automatic single diagnostic after 60 present callbacks; F9 requests another snapshot when idle. An optional Lua status UI reads local status.json using documented REFramework APIs. A worker avoids blocking rendering; the DLL stays pinned until game exit to avoid unloading live code. Remove it only with the game closed. No UI framework or third-party runtime DLL needed. Bound all input lengths; ReadProcessMemory with error checks rather than dereferencing game pointers.

Disk scan scope: mapped PE section raw content, per-section (not overlay and not zero-fill). Memory scope: readable committed spans within SizeOfImage. Both scopes and holes are reported; snapshots are not atomic. Log counts and bounded RVA candidates, no full memory dumps or account paths. A byte match is never named 'compatible'. Other mods may explain differences but are not automatically blamed.

Tests: original CT manifest extraction, wildcard search and chunk boundaries, PE/RVA validation, SHA-256 vectors, synthetic input scenarios, Windows PE build/export/import validation. No actual RE7 or Steam Deck runtime exists in this environment.
