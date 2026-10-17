# Native hook probe implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans. User already authorized uninterrupted implementation/publication.

**Goal:** Deliver the first native original-hook build at the point where game-device evidence is required.
**Architecture:** Five pass-through assembly detours, bounded per-site queues, worker analysis/logging, exact-build gate and reversible packaging.
**Tech Stack:** C11, x64 assembly, pinned MinHook v1.3.4, MinGW, Python unittest, shell installer.
**Spec:** docs/superpowers/specs/2026-10-17-native-hook-probe-design.md

## Global Constraints
- No camera/model/game-data changes; label this as hook probe, not playable OTS.
- No API/C calls, allocation, waiting or floating arithmetic inside detours.
- Pin DLL; retain all published trampolines to process exit.
- Exact executable/bytes; block simultaneous Explore; 90-second/8-MiB capture bounds.
- First-person face hiding is optional and outside this increment.

## Review Focus
- Preserve flags, all GPRs, x87, SSE and AVX upper halves on every detour branch.
- Busy/full queue must drop without blocking or writing outside storage.
- Partial hook enable/rollback failure must keep original execution paths valid.
- File/byte/build mismatch must reject before first hook enable.
- Uninstall must not overwrite unrelated or modified user files.

## Task 1: Pass-through detours and bounded capture
Files: src/probe_capture.h/.c/.S; tests/test_probe_capture.py and native harness.
Interface: five queues, trampolines, enabled/frame/sequence globals, probe_drain.
- [x] Write/run failing assembly preservation, disabled/full/busy/queue tests.
- [x] Implement detours and drain; run host harness and Windows compile.

## Task 2: Validated hook lifecycle and plugin
Files: src/probe_sites.h, probe_lifecycle.c/.h, probe_plugin.c; probe_build.sh;
tools/fetch_minhook.py + pinned hashes; tests/test_probe_lifecycle.py and binary tests.
Interface: injected create/enable/disable/verify callbacks; single-install state;
worker owns state, present only increments frame and posts capture request.
- [x] Test create/enable failure, mismatch and retaining live trampolines before implementation.
- [x] Implement exact-build gate, worker, capture log/summary and controls.
- [x] Verify relocations for five actual prefixes; run complete offline suite.

## Task 3: Device package and delivery
Files: probe/install.sh, uninstall.sh, README_KO.md; tools/package_probe.py;
workflow package step; tests/test_probe_package.py.
- [x] Test backup/install/uninstall, conflicts and unchanged legacy payloads.
- [x] Implement package; document exact device procedure, logs and limits.
- [x] Fresh review, fix Important/Critical with regression tests, full suite.
- [ ] Publish work, verify CI source/binary provenance, deliver test ZIP.
