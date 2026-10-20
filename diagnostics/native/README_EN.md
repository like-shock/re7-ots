# RE7 OTS Native 0.1.15

An experimental native port of the original Third-Person OTS Script RT 8.02 for **Resident Evil 7 Steam RT**, using REFramework on Steam Deck / Proton.

Version 0.1.15 adds camera-position and FOV controls to REFramework while preserving the original defaults. The camera starts OFF on every launch: **F6** toggles third person; **F7** swaps shoulders. No separate Lua installation or Probe run is required.

## Requirements

- RE7 **Steam RT** with a working REFramework installation.
- A separate compatible RT model pack for complete visible character models. See [character/model requirements (Korean)](https://github.com/like-shock/re7-ots/blob/main/docs/NATIVE_CHARACTERS_KO.md). The original CT and external models are not included.
- Follow your model pack's instructions: place its `natives` folder beside `re7.exe` and enable **REFramework → Loose File Loader → Enable Loose File Loader** when required.
- Avoid running the original CT or another camera mod at the same time.

## New installation

1. Exit the game and switch to Steam Deck Desktop Mode.
2. Extract this ZIP to a folder separate from the game. Open a terminal in the extracted folder.
3. Replace the path below with the game folder containing `re7.exe` and run:

   ```bash
   bash install.sh "/path/to/game-folder"
   ```

4. Launch the game and press F6. Map the keys to Steam Deck buttons or configure the controller shortcuts below.

The installer verifies SHA256SUMS and backs up recognized older Explore, Observer, Diagnostics and NativeProbe files. Models, settings and logs are preserved. It refuses to overwrite an existing Native DLL or installation receipt; use the update procedure below.

## Updating

Exit the game. For a script-managed installation, run the stored uninstaller first. Then run the new installer from the newly extracted ZIP folder:

```bash
bash "/path/to/game-folder/reframework/data/RE7_OTS_Native/install/uninstall.sh" "/path/to/game-folder"
bash install.sh "/path/to/game-folder"
```

If you copied the DLL manually, first move the old `reframework/plugins/RE7_OTS_Native.dll` outside the game folder. Do not keep both versions in the plugins folder.

For manual installation, copy the ZIP's `reframework/plugins/RE7_OTS_Native.dll` into the same path under the game folder. Keep older OTS/Explore/Probe DLLs and `reframework/autorun/re7_ots_explore.lua` in a separate backup location. Manual installation does not create an uninstaller receipt.

## Settings

Open **REFramework → Script Generated UI → RE7 OTS Native**. Changes are saved automatically and survive restart/F6 transitions. The settings file is `reframework/data/RE7_OTS_Native/settings.cfg`.

| Option | Default and behavior |
| --- | --- |
| Auto-Disable with Melee Weapons | ON. Automatically uses first person for melee weapons. Other automatic transitions remain when OFF |
| Hide Head in First Person | ON. Also applies with F6 OFF |
| Hide Mia Hair in First Person | ON under the head-hiding setting. Can be disabled for incompatible custom hair |
| Limit Light Source Contrast in Third Person | ON. Limits third-person light contrast; Fullbright uses its own brightness override while enabled |
| Force Custom Screen Shader / Custom Screen Shader | OFF / Main Menu. Applies with F6 ON; critical-health effects take priority |
| Fullbright | OFF. Supports F6 ON/OFF. Uncheck to restore normal lighting |
| Fullbright Red / Green / Blue / Brightness | RGB 255/255/255, brightness 1. Color range 0–255; brightness 0–10 |
| Controller Camera Toggle / Shoulder Switch | Unassigned. Bind camera-toggle and shoulder-swap inputs |

Release and press the controller button again after binding, loading or reconnecting. Binding both actions to the same input performs both. Keyboard F6/F7 remain available.

### Camera and FOV controls (0.1.15)

Existing preferences are retained; new controls start with the defaults below. Changes survive F6 transitions and restart. Distance and shoulder are multipliers of the original camera; height adds a third-person-only offset.

| Option | Default / range |
| --- | --- |
| Camera Distance Scale | 1 / 0.5–2 |
| Shoulder Offset Scale | 1 / 0–2. F7 swaps direction |
| Camera Height Offset | 0 / -0.5–0.5. Positive moves upward |
| Use Custom FOV / Custom FOV | OFF / 110. Custom range 60–120 degrees |
| First-Person Running FOV / Third-Person Running FOV | OFF / ON |
| Running FOV Increase (%) | 20 / 0–40 |
| Running FOV Rise (seconds) / Fall (seconds) | 7 / 0.7. Rise 0.1–10 seconds; fall 0.1–5 seconds |

FOV controls apply **while F6 is ON**. First-person running FOV refers to automatic first-person transitions; F6 OFF returns control to the game / REFramework. Custom FOV OFF follows the game menu. Existing handoff when REFramework writes FOV is preserved.

Default flashlight, environmental audio, head-bob, blur, VHS, blood and HUD corrections still follow the original policies automatically.

## Known issues

- 0.1.12 fixed block-shaped bright patches in distant dark areas when rotating with Fullbright enabled; improvement was confirmed in the reported scene.
- **Unresolved:** Healing with Fullbright enabled can darken the entire screen during the automatic first-person animation. Brightness returns when third person resumes.
- RELit's camera-attached light can make the character look dark or color-inverted. Disabling that light resolved the reported setup.
- Not every area, DLC or Steam Deck performance scenario is verified. NAH oxygen/transmission HUD, aiming/running FOV, other characters and controller reconnection need further device checks.
- 0.1.13 is retained on a comparison branch following a third-person regression and is not the current gameplay baseline.

## Reporting problems and uninstalling

Record the area and settings used, then keep both logs from the same game session:

```text
Game folder/reframework/data/RE7_OTS_Native/native_*.jsonl
Game folder/reframework.log
```

After exiting the game, run only the uninstaller command from the update procedure to restore files backed up by the installer. Restoration stops if the installed DLL or backup was modified. Settings, logs and models are preserved. For a manual installation, move out the Native DLL and restore the files you backed up yourself.

See `ORIGINAL_CREDITS.txt` for original and model authors, and `NOTICE.md` / `LICENSE` for terms. Korean instructions are in `README_KO.md`.
