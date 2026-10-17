# Attribution

Native Hook Probe 0.1.0 and Native OTS 0.1.0 use MinHook v1.3.4, revision
`c3fcafdc10146beb5919319d0683e44e3c30d537` (TsudaKageyu/minhook).
Its BSD notices, including the HDE decoder attribution, are reproduced in
MINHOOK_LICENSE.txt. Build inputs are individually hash-pinned in data/minhook.json.
The probe installs process-local instruction detours and replays the original
instructions. It does not modify camera/model/firing data or executable files.

Byte patterns, restore-byte reference data, symbol names and offsets were extracted from the user-provided **Third-Person OTS Script — RE7 RT 8.02** (`re7.CT`, SHA-256 `4154ee807842eb17f51b84272c98c3fdf153082c252f176446aa0cd61fd0686d`). Original mod page: https://www.nexusmods.com/residentevil7/mods/12 . The included ORIGINAL_CREDITS.txt preserves the accompanying acknowledgements and the original author's invitation to reuse the script. This package is an independent camera prototype and diagnostic collection, not an official OTS or REFramework release. Explore v0.3.0 intentionally changes the camera position through the REFramework API. It contains no game executable, character model or game asset, and does not activate the original CT patches.

REFramework / praydog supplies the plugin loader and documented API contract. A minimal, manually declared ABI prefix is used; the full framework is not bundled. References:
- https://refdocs.praydog.com/
- https://github.com/praydog/REFramework/blob/master/include/reframework/API.h
- https://refdocs.praydog.com/api/json.html
- https://refdocs.praydog.com/api/re.html
- https://learn.microsoft.com/windows/win32/api/memoryapi/nf-memoryapi-readprocessmemory
- https://learn.microsoft.com/windows/win32/api/memoryapi/nf-memoryapi-virtualquery
- https://learn.microsoft.com/windows/win32/api/libloaderapi/nf-libloaderapi-getmodulehandleexw

The original diagnostic implementation (excluding original mod-derived reference data) is provided under the MIT License in LICENSE. No third-party license is replaced by this grant.

The `original_scalar/state/collision/interaction/model/inputs` native-port modules
and integrated `native_runtime.c` derive their behavior from cheezeit / goldedtoad's
RT 8.02 script. Native OTS connects these modules to process-local instruction
hooks and intentionally changes camera, firing, inspection and transparency
data; this differs from the pass-through Probe. Original acknowledgements and
reuse terms in ORIGINAL_CREDITS.txt apply
to this derived work. The private-CT test extracts only numeric instructions
locally; neither the CT nor its generated oracle is included in distribution.


Runtime v0.5.0-dev includes independently implemented bindings based on supplied RT model metadata
and public API usage in alphaZomega's EMV Engine (commit a8a89961fde640d542afc72bd66fd8e12f3b82b3).
The full RT placeholder archive credits AfroJackX. Mesh metadata was inspected using NSACloud's
RE-Mesh-Editor (commit 622daa75b41ec622c3444ecb44ef31a651692687); its parser code and all model assets
are external and are not copied or bundled here. See docs/RUNTIME_EXPANSION_KO.md in the source tree
for API references, exact model scope and unverified runtime limits.


## Lua 5.4.3 core (Explore native 0.4.0)

The Explore DLL statically links the Lua core, following REFramework's example plugin.
Pinned source: https://www.lua.org/ftp/lua-5.4.3.tar.gz
SHA-256: `f8612276169e3bfcbcfb8f226195bfc6e466fe13042f1076cbde92b7ec96bbfb`.
The core source is fetched into the build cache, not copied into this repository.
Reference REFramework revision: `d052e8fd3fd41d0a1a33394fcf6e6c19be39b1ee`.
The Windows DLL also uses the system msvcrt runtime; no Lua DLL or runtime installer is bundled.

Copyright (C) 1994-2021 Lua.org, PUC-Rio.

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
