# Native follow-ups: approved scope

The user requested these three groups in order: release automation plus package documentation; REFramework settings; Fullbright diagnostics. Each group receives a new build version. Keep `main`, `version/0.1.12` and `version/0.1.13`; preserve the two version branches and existing releases.

1. **0.1.14:** build stable version tags through GitHub Actions, validate the exact tested Native package, and publish a version-only release title with English/Korean notes and ZIP/checksum assets. Update packaged Korean instructions and add English instructions. Fullbright healing remains explicitly unresolved.
2. **0.1.15:** expose camera distance, shoulder offset, height, custom FOV and running FOV controls in the existing embedded REFramework menu. Preserve original defaults and existing saved preferences; validate input ranges and persist changes across restart/F6. Preserve current camera policy and REFramework FOV conflict handling.
3. **0.1.16:** add bounded Fullbright state/reason diagnostics around view transitions, preserving 0.1.12 lighting decisions. Record enough to distinguish readiness loss, target invalidation and missing producer calls during healing. Do not claim a healing fix without Steam Deck evidence.

Windows builds and the complete offline suite run in GitHub Actions. Local host tests are permitted; no local game testing is assumed. Existing installation receipts, logs and external models must remain intact.
