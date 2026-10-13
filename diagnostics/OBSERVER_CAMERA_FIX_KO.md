# RE7 OTS Observer v0.2.1 — 카메라 조회 수정

기존 v0.2.0 설치에 적용하는 **Lua 한 파일 교체본**입니다. native DLL은 v0.2.0 그대로 사용합니다.

## 적용

1. 게임을 완전히 종료합니다.
2. ZIP의 `reframework/` 폴더를 `re7.exe`가 있는 게임 폴더에 합쳐 `reframework/autorun/re7_ots_observer.lua`를 덮어씁니다.
3. 기존 `reframework/plugins/RE7_OTS_Observer.dll`을 유지하고 평소 설정으로 게임을 실행합니다.
4. Script Generated UI의 관찰기 이름에 **v0.2.1**이 표시되는지 확인합니다.

이 ZIP에는 DLL이 없습니다. v0.2.0 관찰기를 먼저 설치한 상태에서 사용하세요. 새 카메라 로그의 버전은 0.2.1, native 로그의 버전은 0.2.0인 것이 정상입니다.

## 이전 로그에서 확인한 내용

세션: `re7_observer_2026_10_13_11_59_49_4310882_1`.
사용자의 “run none desktop mode” 설명을 데스크톱 모드가 아닌 실행으로 해석했습니다. SteamOS/Proton 버전은 이 로그로 확인하지 못했습니다.

| 항목 | 관찰 결과 |
|---|---|
| 기록 | native 1,451개, 카메라 1,129개. 두 파일 모두 종료 레코드 있음 |
| 실행 파일 | 알려진 SHA-256 및 PE 정보 일치 |
| 플레이어 좌표 | MOVE에서 변화, LOOK/AIM에서 고정 |
| 조준 후보 | AIM에서 0→1→0 한 번, 1인 샘플 5개. 반복 조준 검증은 부족 |
| 인벤토리 후보 | INVENTORY에서 0→1→0 두 번 |
| 상호작용 후보 | EXAMINE과 INVENTORY 양쪽에서 비영 값. 물체 검사 전용 플래그라고 확정할 수 없음 |
| 재로딩 구간 | 106.082초에 player/aim NULL_LINK. player 112.936초, aim 122.318초에 generation 2로 읽기 재개 |
| 카메라 | 1,129개 전부 UNKNOWN_TRANSFORM_METHOD, 좌표·회전 미수집 |

시각은 첫 native 샘플 기준입니다. 플레이어 좌표가 조준 상태보다 9.382초 먼저 읽혔으므로, 향후 카메라 적용의 복귀 조건을 좌표 읽기 성공만으로 판단할 수 없습니다.
단계 라벨은 사용자 선택값이며, 기록된 후보 값의 의미나 게임 객체 종류를 자동으로 증명하지 않습니다.

## 수정과 검증

v0.2.0은 Camera component에서 `get_Transform`을 직접 찾았습니다. v0.2.1은 매 샘플마다 `primary_camera → get_GameObject → get_Transform` 순서로 조회합니다. GameObject가 없거나 메서드를 찾지 못하면 원인을 기록하고 좌표를 남기지 않습니다.

기존 모의 카메라가 잘못된 직접 getter를 제공해 이 결함을 놓쳤습니다. 모의 객체를 실제 getter 관계에 맞춰 수정했고, 수정 전 4개 테스트 실패를 확인한 뒤 구현을 고쳤습니다. 수정 후 Python/Lua 검사 28개, C 핵심 검사 30개, 관찰 코어 12개 시나리오가 통과했습니다. 별도 코드 검토에서도 새 문제는 보고되지 않았습니다.

이 결과는 **개발 환경의 모의 검증**입니다. 수정한 카메라 경로의 실제 Steam Deck 동작, CT 카메라 대상과의 동일성, 3인칭 카메라 적용은 아직 검증하지 않았습니다. 이 버전은 계속 읽기 전용 관찰기입니다.

## 이번에는 짧게 확인

이전에 쓰신 실행 모드와 설정 그대로 진행해 주세요.

1. 조작 가능한 상태에서 **F8** → 약 5초 정지합니다.
2. 제자리에서 시점만 약 10초 돌리고, 이어서 약 10초 이동합니다.
3. 세이브를 한 번 다시 불러온 뒤, 조작 가능해지면 정지·회전·이동을 약 10초 수행합니다.
4. **F8**로 종료하고, 새 세션의 `_native.jsonl`과 `_camera.jsonl` 두 파일을 전달합니다.

이번 짧은 시험은 F7 단계 변경 없이 위 순서로 진행해도 됩니다. 가능하면 재로딩을 시작한 대략적인 시점을 알려주세요. 이전 인벤토리·물체 검사 단계를 모두 반복할 필요는 없습니다.
REFramework UI에서 Camera가 `READABLE_CANDIDATE`로 바뀌면 조회는 성공한 것이며, 실제 좌표·회전의 대응은 새 로그로 확인합니다.

로그 폴더: `reframework/data/RE7_OTS_Observer/`.

조회 경로 참고: [RELit의 카메라 조회](https://github.com/originalnicodr/RELit/blob/main/RELit.lua), [REFramework FreeCam](https://github.com/praydog/REFramework/blob/master/src/mods/FreeCam.cpp). 참고 프로젝트의 소스는 이번 교체 ZIP에 포함하지 않습니다.
