# v0.5.1-dev: 실기 피드백 반영

에단 모델 표시와 좌우 어깨 전환은 사용자 확인 완료. 총기 조준, 자동 머리 숨김/표시는 아직 미확인이다.

| 보고 | 근거와 이번 처리 | 남은 확인 |
|---|---|---|
| 벽 옆에서 FP/OTS 반복 | 로그의 7회는 WAIT_NATIVE_INVALID. 당시 충돌 기능 OFF. nil/예외 읽기에 한해 즉시 1회 재시도 | 실제 파일 읽기 원인과 반복 전환 감소 여부 |
| 머리 제어 대기 | WAIT_MODEL_COMPONENTS949 표본. SystemArray의 C++ vector를 Lua table로 제한한 코드 결함 수정 | 실제 바인딩 성공과 FP에서 숨김 |
| A버튼 위치 어긋남 | 선택 가능한 PrerenderGUI 적용 경로, 이후 LockScene 소유권 확인 추가 | 실제 GUI 계산 시점과 옵션의 효과 |
| 가운데 조작 기준 어긋남 | 이동 전후 시선은 평행하며 원점이 달라 시차가 발생. 카메라 단계별 좌표 기록 | 실제 상호작용 목표/화면 투영과 선택 방향 보정 |
| 벽에서 손이 목 뒤에 표시 | PlayerHandTouch.HandIK 목표와 PlayerMotionController 상태를 읽는 진단 추가 | 어느 메시/애니메이션인지 확인 후 안전한 보정 |

두 로그는 같은 Lua 실행에 속하며 1,127개 표본, 잘못된 행/중복/충돌 0건이었다. 상태 파일 실패에서
약 67ms 후 새 표본이 나타났지만, 이전 로그는 nil·예외·잘못된 JSON 객체를 구분하지 못한다.
따라서 파일 교체 경쟁을 확정 원인으로 단정하지 않는다. 네이티브는 이미 임시 파일 + 원자적 교체를 사용한다.
재시도는 대기 없이 최대 2회 읽으며, 기존 마지막 표본으로 유효 시간을 연장하지 않는다.
조준·인벤토리·상호작용·로딩·세대·재전송·오래된 상태의 차단 조건은 유지한다.

모델 수정은 정확한 머리 경로/재질과 플레이어 하위 트리 검증을 유지한다. 배열 크기를 먼저 확인하고
0부터 인덱싱한다. 64개 초과, 잘못된 크기, nil 원소는 탐색 중단이며, 빈 배열은 정상적으로 건너뛴다.
게임 몸체, 팔, NPC와 알 수 없는 메시에는 쓰지 않는다.

새 실험 옵션 `Earlier HUD camera (experimental)`은 기본 OFF다. UpdateBehavior에서 원래 위치를
복원한 뒤 PrerenderGUI 전에 한 번 적용한다. LockScene에서 같은 카메라·같은 목표 위치인지 확인한다.
콜백 누락/순서 변경에는 대기, 외부 위치 변경에는 OFF로 전환하며 외부 좌표를 덮어쓰지 않는다.
실제 HUD 수정 완료나 상호작용 조준 보정 완료를 의미하지 않는다.

## 원문 근거

REFramework 참조 커밋 `d052e8fd3fd41d0a1a33394fcf6e6c19be39b1ee`:

- `shared/sdk/SystemArray.hpp/.cpp`, `src/mods/bindings/Sdk.cpp`: get_elements는 C++ vector 반환,
  get_size/get_element는 직접 제공되는 배열 API.
- `src/mods/bindings/Json.cpp`: 파일 파싱 예외가 Lua nil로 반환된다.
- `shared/sdk/Enums_Internal.hpp`: PrerenderGUI는 LockScene보다 앞에 있다. 실제 RE7 HUD 갱신 단계의 증명은 아니다.
- `scripts/utility/RE7.lua`: ObjectManager.PlayerObj, PlayerHandTouch.HandIK, Target,
  PlayerMotionController.IsRArmJacked/IsPosturalCameraControl.
- `src/mods/VR.cpp`: RE7 UIWorldPosAttach._NowTargetPos와 InteractOperationCursor의 존재를 확인할 수 있다.
  VR의 GUI 공간 변경/행렬 쓰기는 일반 카메라에 그대로 이식하지 않았다.

EMV 참조 커밋 `a8a89961fde640d542afc72bd66fd8e12f3b82b3`, `EMV Engine/init.lua`는
world_to_screen에 맞춰 Camera 관절과 Transform을 일시 정렬한다. 이 차이를 기록하지만,
현재 진단은 view/projection 행렬이나 실제 A버튼 스크린 좌표까지 측정하지 않는다.

## 검증

Lua SDK 경계 검사에 실제 SystemArray의 인덱스 API를 반영했다. 파일 읽기 일회/연속 실패,
재시도 후 조준·세대 변경·재전송, 배열 한도/중복/교체, GUI 전후 단계·중복/누락/재정렬,
다른 카메라·외부 위치 변경, 진단 오류 격리를 검사한다. 원본 CT를 사용하는 선택 검사도 로컬에서 실행한다.
실제 화면과 손 애니메이션은 위 표의 미확인 항목으로 남긴다. 사용자 원본 로그와 모델 파일은 배포하지 않는다.
