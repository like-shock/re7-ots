# RE7 OTS Diagnostics Implementation Plan

**Goal:** Diagnose RT OTS patch candidates locally without uploading game executables.
**Architecture:** Portable C read-only scanner + freestanding Windows x64 REFramework wrapper; generated immutable manifest from the uploaded CT.
**Tech Stack:** C11, Python standard library, clang/lld, Kernel32 APIs.
**Spec:** DESIGN.md

## Tasks
- [x] Test and implement original CT extraction; reproduce 78 sites and compare manifest to earlier inventory.
- [x] Test and implement masked search, overlap handling, PE bounds/mapping and SHA-256.
- [x] Implement DLL initialization, readable-region snapshots, local-file hashing and incremental logs. No code patches.
- [x] Test malformed inputs and synthetic snapshots; compile Windows DLL; inspect PE exports/imports.
- [x] Package DLL, Korean instructions, source, manifest, reproducible build and verification evidence.

## Review focus
False unique matches across chunk boundaries; raw offset vs RVA confusion; unreadable memory treated as successful full scan; current hooks changing AOBs; DLL unloading while a scan is active.
