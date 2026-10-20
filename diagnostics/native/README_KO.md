# RE7 OTS Native 0.1.15

RE7 Steam RT용 3인칭 어깨너머 시점 모드입니다. 원본 Third-Person OTS RT 8.02를 REFramework 네이티브 플러그인으로 이식한 실험 버전이며, Steam Deck / Proton을 기준으로 개발합니다.

0.1.15는 REFramework에서 카메라 위치와 FOV를 조절할 수 있도록 설정을 추가합니다. 기존 기본값은 유지합니다. 게임을 시작하면 카메라는 OFF이며 **F6**으로 켜고, **F7**로 어깨 방향을 바꿉니다. 별도 Lua 파일 설치나 Probe 실행은 필요하지 않습니다.

## 준비

- RE7 **Steam RT 버전**과 정상 작동하는 REFramework가 필요합니다.
- 머리까지 완전한 모델을 표시하려면 별도의 RT 모델 팩이 필요합니다. [캐릭터·모델 안내](https://github.com/like-shock/re7-ots/blob/main/docs/NATIVE_CHARACTERS_KO.md)를 참고하세요. 원본 CT와 외부 모델은 포함하지 않습니다.
- 모델 팩의 `natives` 폴더는 해당 모델의 설치 안내에 따라 `re7.exe` 옆에 배치하고 REFramework의 **Loose File Loader → Enable Loose File Loader**를 켭니다.
- 원본 CT나 다른 카메라 모드를 동시에 실행하지 마세요.

## 새로 설치

1. 게임을 종료하고 Steam Deck의 데스크톱 모드로 전환합니다.
2. ZIP을 게임 폴더와 별도의 폴더에 풉니다. 압축을 푼 폴더에서 터미널을 엽니다.
3. 아래 경로를 `re7.exe`가 있는 폴더로 바꿔 실행합니다.

   ```bash
   bash install.sh "/path/to/game-folder"
   ```

4. 게임을 실행하고 F6을 누릅니다. Steam Deck에서는 키를 버튼에 할당하거나 아래 컨트롤러 단축키를 설정하세요.

설치기는 SHA256SUMS를 검사하고, 지정된 이전 Explore·Observer·Diagnostics·NativeProbe 파일을 백업합니다. 설치된 모델과 설정·로그는 유지됩니다. 기존 Native DLL이나 설치 기록이 있으면 덮어쓰지 않으므로 아래 업데이트 절차를 따르세요.

## 업데이트

게임을 종료합니다. 설치 스크립트로 설치한 경우 기존 설치 기록에 보관된 제거 스크립트를 먼저 실행하고, 새 ZIP을 푼 폴더에서 새 설치 스크립트를 실행합니다.

```bash
bash "/path/to/game-folder/reframework/data/RE7_OTS_Native/install/uninstall.sh" "/path/to/game-folder"
bash install.sh "/path/to/game-folder"
```

기존 DLL을 수동 복사해 설치했다면 게임의 `reframework/plugins/RE7_OTS_Native.dll`을 게임 폴더 밖으로 옮긴 뒤 설치하세요. 이전 DLL과 새 DLL을 plugins 폴더에 함께 두지 마세요.

수동 설치를 선택한다면 ZIP의 `reframework/plugins/RE7_OTS_Native.dll`을 같은 게임 경로에 복사하고, 이전 OTS/Explore/Probe DLL 및 `reframework/autorun/re7_ots_explore.lua`는 별도로 보관하세요. 수동 설치에는 제거 스크립트용 설치 기록이 생성되지 않습니다.

## 설정

**REFramework → Script Generated UI → RE7 OTS Native**에서 변경합니다. 설정은 자동 저장되며 게임 재시작과 F6 전환 후에도 유지됩니다. 설정 파일은 `reframework/data/RE7_OTS_Native/settings.cfg`입니다.

| 메뉴 | 기본값·동작 |
| --- | --- |
| Auto-Disable with Melee Weapons | ON. 근접 무기 사용 시 자동 1인칭. OFF여도 다른 자동 전환 조건은 유지 |
| Hide Head in First Person | ON. F6 OFF에서도 1인칭 머리 숨김 적용 |
| Hide Mia Hair in First Person | ON. 머리 숨김이 켜져 있을 때 적용. 사용자 모델과 충돌하면 이 항목만 OFF 가능 |
| Limit Light Source Contrast in Third Person | ON. 3인칭 광원 대비 제한. Fullbright 사용 중에는 별도 밝기 보정이 우선 |
| Force Custom Screen Shader / Custom Screen Shader | OFF / Main Menu. F6 ON에서 강제 필터 적용. 빈사 효과 우선 |
| Fullbright | OFF. F6 ON/OFF 모두 지원. 체크 해제 시 원래 조명으로 복귀 |
| Fullbright Red / Green / Blue / Brightness | RGB 255·255·255, 밝기 1. 각 색 0~255, 밝기 0~10 |
| Controller Camera Toggle / Shoulder Switch | Unassigned. 카메라·어깨 전환 버튼 지정 |

컨트롤러 버튼은 지정·로드·재연결 후 놓았다가 다시 눌러야 합니다. 같은 버튼에 두 동작을 지정하면 둘 다 실행됩니다. F6/F7은 계속 사용할 수 있습니다.

### 카메라·FOV 조절 (0.1.15)

기존 설정 파일은 유지하고 새 항목만 아래 기본값으로 시작합니다. 변경값은 F6 전환과 재시작 후에도 유지됩니다. 카메라 거리·어깨 값은 원본 기준 배율이며 높이는 3인칭에만 더하는 오프셋입니다.

| 메뉴 | 기본값 / 범위 |
| --- | --- |
| Camera Distance Scale | 1 / 0.5~2 |
| Shoulder Offset Scale | 1 / 0~2. F7로 방향 전환 |
| Camera Height Offset | 0 / -0.5~0.5. 양수는 위로 이동 |
| Use Custom FOV / Custom FOV | OFF / 110. 사용자 지정 범위 60~120도 |
| First-Person Running FOV / Third-Person Running FOV | OFF / ON |
| Running FOV Increase (%) | 20 / 0~40 |
| Running FOV Rise (seconds) / Fall (seconds) | 7 / 0.7. 증가 0.1~10초, 복귀 0.1~5초 |

FOV 설정은 **F6 ON 동안** 적용합니다. 1인칭 달리기 항목도 자동 1인칭 전환 상태에 대한 설정이며, F6 OFF에서는 게임·REFramework가 FOV를 제어합니다. 사용자 지정 FOV가 OFF면 게임 메뉴 값을 사용합니다. REFramework가 FOV를 쓰면 양보하는 기존 처리는 유지됩니다.

손전등·환경음·화면 흔들림·블러·VHS·혈액·HUD 등 기본 보정은 원본 정책에 따라 자동 적용됩니다.

## 알려진 문제

- 0.1.12에서 Fullbright 회전 시 원거리의 각진 밝은 경계 문제를 수정했으며 보고된 장면에서 개선이 확인되었습니다.
- **미해결:** Fullbright 사용 중 치료 아이템으로 자동 1인칭 전환되면 화면 전체가 어두워졌다가 3인칭 복귀 시 다시 밝아질 수 있습니다.
- RELit의 카메라 부착 조명을 함께 사용하면 캐릭터가 어둡거나 색이 뒤집혀 보일 수 있습니다. 보고된 환경에서는 해당 조명을 끄면 해소되었습니다.
- 모든 구간·DLC·Steam Deck 성능을 확인한 상태는 아닙니다. NAH 산소·통신 HUD, 조준·달리기 FOV, 다른 캐릭터와 컨트롤러 재연결은 추가 실기 확인 대상입니다.
- 0.1.13은 3인칭 동작 문제로 사용 기준에서 제외했으며 비교용 브랜치로 보존합니다.

## 문제 보고·제거

문제가 발생한 구간과 사용 설정을 적고, 같은 실행의 아래 두 로그를 보관하세요.

```text
게임폴더/reframework/data/RE7_OTS_Native/native_*.jsonl
게임폴더/reframework.log
```

게임 종료 후 위 업데이트 절차의 제거 명령만 실행하면 설치기가 백업한 이전 파일을 복원합니다. 설치한 DLL이나 백업이 변경되어 있으면 복원을 중단합니다. 설정·로그·모델은 삭제하지 않습니다. 수동 설치를 제거할 때는 직접 설치한 Native DLL을 옮기고 별도 보관한 파일을 복원하세요.

원본 제작자·모델 제작자 고지는 `ORIGINAL_CREDITS.txt`, 이용 조건은 `NOTICE.md`와 `LICENSE`를 참고하세요. 영어 안내는 `README_EN.md`에 있습니다.
