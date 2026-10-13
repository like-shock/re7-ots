# Attribution

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
