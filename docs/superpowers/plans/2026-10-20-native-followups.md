# Native follow-ups Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Complete the three user-selected groups in order, with separate tested releases.
**Architecture:** Extend the existing Actions pipeline, atomic option persistence and worker-owned diagnostics. Preserve runtime defaults and the 0.1.12 lighting policy.
**Tech Stack:** C11, embedded Lua 5.4, Python unittest, GitHub Actions, GitHub CLI.
**Spec:** `docs/superpowers/specs/2026-10-20-native-followups.md`.

## Global Constraints

- Versions 0.1.14, 0.1.15 and 0.1.16, in that order; existing version branches remain frozen.
- Titles contain only the version; release notes use English and Korean without verification sections.
- Fullbright healing remains unresolved; diagnostics must not relax game-memory/readiness checks.
- GitHub Actions performs Windows builds and the complete suite; targeted host tests run locally.
- Work in the existing dedicated checkout on main, preserving the user's three-branch policy.

## Review Focus

- Incorrect tag, stale artifact or damaged checksum must prevent publication.
- Older settings files and in-flight save requests must preserve the user's preferences.
- F6 reset, area/load transitions and REFramework FOV writes must retain the intended ownership.
- Diagnostic volume must remain bounded without changing hook outputs or blocking gameplay.
- A failed release publication must not replace existing release assets.

### Task 1: Release automation and package guides (0.1.14)

**Files:** `.github/workflows/offline-build.yml`, `diagnostics/tools/prepare_native_release.py`, `diagnostics/tools/package_native.py`, `diagnostics/native/README_{KO,EN}.md`, `diagnostics/tests/test_native_release.py`, `diagnostics/tests/test_native_package.py`, version markers, `README.md`, `docs/release-notes/0.1.14.md`.
**Interfaces:** `prepare(project, artifacts, tag, commit) -> dict` produces validated version, ZIP, checksum and notes paths for the release job. Later tasks supply matching versions/notes to this same interface.

- [x] Add failing tests for valid release preparation, wrong tag/version/commit, dirty builds, duplicate/missing ZIP entries and corrupt payload hashes. Extend package test to require both language guides.
- [x] Run `python3 -m unittest discover -s diagnostics/tests -p 'test_native_release.py' -v`; expect missing implementation failure.
- [x] Implement preparation and a tag-only release job with read-only build permissions, separate write permission, main-ancestry check and no asset overwrite. Refresh both guides and release notes.
- [x] Run release/package tests and `git diff --check`; expect success. Commit, push and await complete Actions success, then tag and verify published title/body/assets.

### Task 2: Camera and FOV settings (0.1.15)

**Files:** `diagnostics/src/native_options.{h,c}`, `native_options_ui.c`, `native_plugin.c`, runtime/FOV interfaces as needed; host settings/UI/FOV tests; both guides and release notes.
**Interfaces:** append named settings to the existing atomic requested/completed fields; use the worker's `apply_runtime_settings` for runtime updates. Keep old settings formats readable and original defaults unchanged.

- [x] Add failing host tests for round-trip/migration/range checks, save races, actual UI callbacks, runtime application and F6 persistence.
- [x] Run targeted settings/FOV host tests; expect missing settings behavior.
- [x] Expose camera distance/shoulder/height and custom/running FOV settings with finite bounded values. Reset temporary FOV authority when settings change, preserving REFramework grace.
- [x] Run affected tests, bump to 0.1.15, document defaults/ranges and commit. Require Actions success before tagging/release.

### Task 3: Fullbright diagnostic evidence (0.1.16)

**Files:** `diagnostics/src/native_fullbright.{h,c}`, `native_runtime.{h,c}`, `native_plugin.c`, Fullbright host tests, package guides and release notes.
**Interfaces:** hook-owned state/counters copied under the existing lock into `NativeSnapshot`; worker formats bounded transition records, never hook-side I/O.

- [x] Add failing tests for readiness-loss reasons, light invalidation/recovery, mode/view transition evidence and unchanged register/memory effects.
- [x] Implement diagnostics only; retain the 0.1.12 authorization and lighting calculations.
- [ ] Run focused Fullbright/lifecycle tests and complete Actions suite; bump/document 0.1.16 and collect fresh final review before final publication.
- [ ] Verify all three releases, preserved branches and clean checkout. Report the Steam Deck healing-log capture step.
