# RE7 OTS

Experimental native over-the-shoulder camera for **Resident Evil 7 — Steam RT**, using REFramework on Steam Deck / Proton. Based on the original Third-Person OTS Script RT 8.02.

**RE7 Steam RT용 3인칭 어깨너머 시점 모드**입니다. 원본 RT 8.02 스크립트를 REFramework 네이티브 플러그인으로 이식하며, Steam Deck / Proton을 기준으로 개발합니다.

**Current release / 현재 배포 버전: [0.1.15](https://github.com/like-shock/re7-ots/releases/tag/v0.1.15)**

[Download ZIP / 설치 파일 받기](https://github.com/like-shock/re7-ots/releases/download/v0.1.15/RE7_OTS_Native_v0.1.15_SteamDeck.zip) · [All releases / 전체 릴리스](https://github.com/like-shock/re7-ots/releases) · [한국어](#한국어) · [English](#english) · [Development](#development)

## 한국어

### 설치

1. **RE7 Steam RT와 정상 작동하는 REFramework**를 준비합니다. 머리까지 완전한 캐릭터 모델을 표시하려면 별도의 RT용 모델 팩이 필요합니다. [캐릭터·모델 안내](docs/NATIVE_CHARACTERS_KO.md)를 참고하세요.
2. 게임을 종료하고 Steam Deck의 데스크톱 모드에서 위 **설치 파일 받기**를 누릅니다. ZIP을 별도 폴더에 풀고, 그 폴더에서 터미널을 엽니다. GitHub의 `Source code` 파일은 설치 패키지가 아닙니다.
3. 아래 명령의 경로를 **`re7.exe`가 있는 게임 폴더**로 바꿔 실행합니다.

   ```bash
   bash install.sh "/path/to/game-folder"
   ```

4. 게임을 실행한 뒤 **F6**으로 3인칭을 켭니다. 게임을 새로 실행할 때 카메라는 항상 OFF로 시작합니다. Steam Deck에서는 키를 버튼에 할당하거나 아래의 컨트롤러 단축키 설정을 사용하세요.

설치기는 기존 Explore/Probe 등 지정된 이전 파일을 백업합니다. 모델 파일은 유지됩니다. 원본 CT나 다른 카메라 모드를 동시에 실행하지 마세요. 수동 설치와 복원 방법은 [상세 설치 안내](diagnostics/native/README_KO.md)를 참고하세요.

### 기존 버전에서 업데이트

설치 스크립트로 설치했다면 **게임을 종료하고 기존 제거 스크립트를 먼저 실행**합니다. 그런 다음 새 ZIP을 푼 폴더에서 새 설치 스크립트를 실행합니다.

```bash
bash "/path/to/game-folder/reframework/data/RE7_OTS_Native/install/uninstall.sh" "/path/to/game-folder"
bash install.sh "/path/to/game-folder"
```

설정·진단 로그·모델 파일은 유지됩니다. 기존 DLL을 수동 복사해 설치했다면 [상세 설치 안내](diagnostics/native/README_KO.md)의 수동 업데이트 절차를 따르세요.

### 조작·설정

설정 위치: **REFramework → Script Generated UI → RE7 OTS Native**. 메뉴에서 변경한 옵션은 자동 저장됩니다.

| 조작 / 옵션 | 동작 |
| --- | --- |
| **F6 / F7** | 3인칭 켜기·끄기 / 어깨 방향 전환 |
| **Auto-Disable with Melee Weapons** | 근접 무기 사용 시 자동 1인칭 전환. 기본 ON |
| **Hide Head in First Person / Hide Mia Hair** | 1인칭 머리 숨김과 Mia 머리카락 설정. 기본 ON이며 Mia 옵션은 머리 숨김 설정 아래에서 적용 |
| **Fullbright** | 주변을 밝히는 선택 기능. 기본 OFF이며 3인칭과 F6 OFF 상태의 1인칭에서 사용 가능 |
| **컨트롤러 단축키** | 카메라·어깨 전환 버튼 지정. 기본 미할당 |

0.1.15부터 카메라 거리·어깨 간격·높이와 FOV·달리기 FOV를 같은 메뉴에서 조절합니다. 세부 범위와 사용자 지정 필터 등은 [Native 사용 안내](diagnostics/native/README_KO.md)에 설명되어 있습니다. 별도의 Lua 파일 설치나 Probe 실행은 필요하지 않습니다.

### 현재 상태·알려진 문제

- **0.1.12:** Fullbright를 켜고 카메라를 회전할 때 먼 어두운 영역에 각진 밝은 경계가 나타나던 문제를 수정했습니다. 보고된 장면에서 개선이 확인되었습니다.
- **미해결:** Fullbright 사용 중 치료 아이템으로 자동 1인칭 전환되면 화면 전체가 어두워졌다가, 3인칭으로 돌아오면서 다시 밝아질 수 있습니다.
- **호환성:** RELit의 카메라 부착 조명과 함께 사용할 때 캐릭터가 어둡거나 색이 뒤집혀 보이는 현상이 보고되었습니다. 보고된 환경에서는 해당 조명을 끄면 해소되었습니다.
- 실험 버전입니다. 모든 구간·DLC·Steam Deck 성능을 확인한 상태는 아닙니다. **0.1.13은 3인칭 동작 문제가 보고되어 비교용 브랜치로만 보존합니다.**

## English

### Installation

1. Use **RE7 Steam RT with a working REFramework installation**. Complete visible character models require a separate matching RT model pack; see [character and model requirements (Korean)](docs/NATIVE_CHARACTERS_KO.md).
2. Exit the game, switch to Steam Deck Desktop Mode, and download the **ZIP linked above**. Extract it to a separate folder and open a terminal there. GitHub's `Source code` archives are not installer packages.
3. Replace the path below with the **game folder containing `re7.exe`**, then run:

   ```bash
   bash install.sh "/path/to/game-folder"
   ```

4. Launch the game and press **F6** to enable third person. The camera starts OFF on every game launch. Map the keys to Steam Deck buttons or configure the controller shortcuts below.

The installer backs up recognized older Explore/Probe files and preserves model assets. Avoid running the original CT or another camera mod at the same time. See the [detailed installation guide (Korean)](diagnostics/native/README_KO.md) for manual installation and restoration.

### Updating an existing installation

For a script-managed installation, **exit the game and run the installed uninstaller first**, then run the new installer from the newly extracted ZIP folder:

```bash
bash "/path/to/game-folder/reframework/data/RE7_OTS_Native/install/uninstall.sh" "/path/to/game-folder"
bash install.sh "/path/to/game-folder"
```

Settings, diagnostic logs and model files are preserved. If you installed the DLL manually, follow the manual update procedure in the detailed guide.

### Controls and settings

Open **REFramework → Script Generated UI → RE7 OTS Native**. Option changes are saved automatically.

| Control / option | Behavior |
| --- | --- |
| **F6 / F7** | Toggle third person / swap shoulders |
| **Auto-Disable with Melee Weapons** | Automatically switch to first person for melee weapons. Default ON |
| **Hide Head in First Person / Hide Mia Hair** | First-person head concealment and Mia hair control. Default ON; the Mia option operates under the head-hiding setting |
| **Fullbright** | Optional scene lighting. Default OFF; supports third person and first person with F6 OFF |
| **Controller shortcuts** | Assign camera-toggle and shoulder-swap buttons. Unassigned by default |

Since 0.1.15, the same menu includes camera distance, shoulder offset, height, custom FOV and running FOV controls. The [Native guide (Korean)](diagnostics/native/README_KO.md) covers FOV, custom filters and other options. No separate Lua installation or Probe run is required.

### Current status and known issues

- **0.1.12:** Fixes block-shaped bright patches in distant dark areas when rotating the camera with Fullbright enabled. Improvement was confirmed in the reported scene.
- **Unresolved:** With Fullbright enabled, healing can darken the entire screen during the automatic first-person transition; brightness returns when third person resumes.
- **Compatibility:** RELit's camera-attached light has been reported to make the character look dark or color-inverted. Disabling that light resolved the appearance in the reported setup.
- This is an experimental port. Not every area, DLC or Steam Deck performance scenario has been verified. **0.1.13 is retained only on a comparison branch following a reported third-person regression.**

## Development

### Branches

| Branch | Purpose / 용도 |
| --- | --- |
| [`main`](https://github.com/like-shock/re7-ots/tree/main) | Current Native development / 최신 Native 작업 기준 |
| [`version/0.1.12`](https://github.com/like-shock/re7-ots/tree/version/0.1.12) | Preserved 0.1.12 source at `81c1ddc` / 0.1.12 원본 기준 보존 |
| [`version/0.1.13`](https://github.com/like-shock/re7-ots/tree/version/0.1.13) | Problematic 0.1.13 source at `0297db6`, for comparison / 문제 버전 보존·비교 |

### Build and package

Pushing a stable version tag such as `v0.1.14` builds and publishes the Native ZIP and `SHA256SUMS.txt` with bilingual notes from `docs/release-notes/<version>.md`. Publication requires a commit integrated into `main`, matching source/build versions and intact package hashes. Existing releases are not overwritten; inspect any failed draft before retrying.

[GitHub Actions](https://github.com/like-shock/re7-ots/actions/workflows/offline-build.yml) builds, runs the offline suite and creates packages on `main` pushes and pull requests. Run artifacts are retained for 14 days; use [Releases](https://github.com/like-shock/re7-ots/releases) for published downloads.

To reproduce the CI build on Linux x86_64, install clang 18, lld 18, a host C compiler, Python 3, Git, liblua5.4 and MinGW x64 GCC (posix). The build downloads pinned MinHook and Lua sources. Commit changes **before building and packaging** so the package can match a clean source commit. From the repository root:

```bash
CC_WINDOWS=clang-18 LINK_WINDOWS=lld-link-18 bash diagnostics/test.sh
python3 diagnostics/tools/package_native.py dist
```

The Native DLL is written to `diagnostics/build/RE7_OTS_Native.dll`; the installer ZIP is written to `dist/`. Tests requiring the private original CT are skipped unless a matching file is supplied with `RE7_CT_FILE=/absolute/path/to/re7.CT`. Offline tests do not establish in-game rendering or performance.

### Documentation

- [Native installation and options / 설치·설정](diagnostics/native/README_KO.md)
- [Character and model requirements / 캐릭터·모델](docs/NATIVE_CHARACTERS_KO.md)
- [Native integration history and limitations / 이식 이력·제한사항](docs/NATIVE_INTEGRATION_KO.md)
- [Previous Explore implementation / 이전 Explore 안내](diagnostics/EXPLORE_README_KO.md)
- [Build workflow / 빌드 구성](.github/workflows/offline-build.yml)

`diagnostics/` retains its historical name and contains the current Native implementation, earlier prototypes and offline tests. Older design and progress documents describe their respective versions. Keep game binaries, private CT files, model archives and user runtime logs out of Git.

## Credits and license

Original Third-Person OTS Script: **cheezeit / goldedtoad**. REFramework: **praydog and contributors**. Model authors and original acknowledgements are preserved in [ORIGINAL_CREDITS.txt](diagnostics/ORIGINAL_CREDITS.txt); see also [NOTICE](diagnostics/NOTICE.md).

The project's original implementation is [MIT licensed](LICENSE). Original mod-derived reference data and external model assets retain their own terms. The game, REFramework binaries, original CT archive and third-party model assets are not redistributed in this repository or the Native installer.
