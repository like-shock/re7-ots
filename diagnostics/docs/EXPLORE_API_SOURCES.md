# API references used for v0.3.0

Consulted 2026-10-13. API documentation guides adapter design; it is not a runtime compatibility result for the user's REFramework build.

- Transform getter/setter and `no_dirty`: https://refdocs.praydog.com/api/types/RETransform.html
- Engine entry and script callbacks: https://refdocs.praydog.com/api/re.html
- Game name, keyboard and UI visibility: https://refdocs.praydog.com/api/reframework.html
- Vector construction: https://refdocs.praydog.com/api/types/Vector4f.html
- UI slider/checkbox contracts: https://refdocs.praydog.com/api/imgui.html
- Reference camera axes, local forward -Z / backward +Z: https://github.com/praydog/REFramework/blob/master/src/mods/FreeCam.cpp
- Atomic file replacement: https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-movefileexw

The camera → GameObject → Transform relationship was also checked against the previously supplied RELit script. No RELit source or assets are included in the Explore installer.
