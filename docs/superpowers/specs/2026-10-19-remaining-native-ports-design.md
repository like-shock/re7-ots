# RE7 OTS 나머지 항목 적용 검토

**다음 적용은 Critter 판별, 3인칭 비네트·색수차, NAH 조준점·빈사 효과를 우선 권장한다.** 혈액 효과·빈사 셰이더·머리 숨김 지연은 그다음 묶음으로 적용할 수 있다. NAH HUD와 FOV는 각각의 의존 기능을 함께 구현해야 한다.

2026년 10월 19일, Native 0.1.5 변경본과 제공된 STEAM RT 8.02 원본을 대조했다. 이번 결과는 적용 검토이며 제품 코드를 추가로 변경하지 않았다. 게임 내 화면과 조작감은 Steam Deck 확인이 필요하다.

## 1 기준과 적용 순서

기준은 [Native 0.1.5 PR 1](https://github.com/like-shock/re7-ots/pull/1)의 커밋 `c21d8ed75c759471fffa83d20b2376b3da407378`이다. 검토 시점에 PR은 아직 병합되지 않았다. 따라서 이 보고서의 적용 상태를 기존 `work` 브랜치나 사용자 설치본의 상태로 해석하면 안 된다.

원본은 첨부 ZIP의 `re7.CT`이며 SHA256은 `4154ee807842eb17f51b84272c98c3fdf153082c252f176446aa0cd61fd0686d`이다. 이하 CT 행 번호는 이 파일 기준이다.

| 순서 | 적용할 묶음 | 판단과 기대 동작 |
| --- | --- | --- |
| 1 | Critter 판별 + 비네트·색수차 + NAH 조준점·붉은 빈사 효과 | 우선 권장. 기존 시점·조준 입력을 활용하며, 기본 OTS 화면과 상태 연결의 차이를 줄인다. 새 패치 지점 5개와 기존 아이템 처리 보완이다. |
| 2 | 혈액 효과 + 빈사 셰이더 + 머리 숨김 지연 | 적용 권장. 체력 입력과 전환 상태의 수명까지 함께 구현한다. 새 지점 2개와 기존 머리 처리 보완이다. |
| 3 | NAH HUD | 별도 적용 권장. 산소·통신·이벤트 경고가 필요한 때에는 반드시 표시하는 원본 예외를 보존한다. 새 지점 3개다. |
| 4 | 원본 FOV | 별도 적용 검토 완료. 조준·달리기·메뉴 FOV와 REFramework 우선권을 한 묶음으로 구현한다. 새 지점 6개와 기존 FOV 처리 확장이다. |
| 5 | 모델별 설정과 선택 기능 | Mia 머리카락 독립 토글은 모델 호환성 개선 후보. 사용자 지정 필터·XInput 확장·Fullbright는 기본 OTS 보완 후 선택한다. |

손전등 4개 경로, 환경음 좌표, 카메라 흔들림, 블러, VHS의 8개 지점은 0.1.5 반영분이므로 다시 미적용 항목에 넣지 않았다. [현재 연결 목록](https://github.com/like-shock/re7-ots/blob/c21d8ed75c759471fffa83d20b2376b3da407378/diagnostics/src/native_sites.h#L48)

## 2 먼저 적용할 기본 보정

### Critter 판별

원본 아이템 훅은 Scrap과 Critter 판별을 매번 초기화하고 이름의 특정 UTF16 위치를 확인한다. Native는 Scrap만 갱신하고 `critterexamine`은 기본값 0에 머문다. EOZ의 Critter 조사 예외가 원본 입력을 받지 못한다.

0.1.5 코드의 실제 실행 분기에 N…22 아이템을 넣은 임시 호스트 검사에서도 값은 0이었다. 인벤토리·위치 자동 전환을 꺼서 해당 조건만 분리하면 현재 값으로 `stopflag=0`, 원본 판별값 1을 공급하면 `stopflag=1`이었다. 기본 인벤토리 자동 전환이 증상을 가릴 수 있으므로 기본 설정의 EOZ가 항상 고장 난다는 뜻은 아니다.

다음 구현에서는 아이템 판별 생산자부터 EOZ 정책까지 연결하고, 다른 아이템 선택·읽기 실패·로드 후에 값이 남지 않는지도 검사해야 한다. 동적으로 변하는 조사 상태를 저장 설정처럼 취급하지 않는 편이 적절하다.

근거: CT 4034–4060, 7785–7799. [현재 아이템 연결](https://github.com/like-shock/re7-ots/blob/c21d8ed75c759471fffa83d20b2376b3da407378/diagnostics/src/native_runtime.c#L291), [Critter 값을 소비하는 정책](https://github.com/like-shock/re7-ots/blob/c21d8ed75c759471fffa83d20b2376b3da407378/diagnostics/src/original_state.c#L107).

### 비네트와 색수차

비네트는 원본 기본값대로 FP는 유지하고 TP는 억제할 수 있다. 색수차도 평상시 FP는 유지하고 TP는 억제하지만, 정신자극제 효과는 별도다. 원본은 정신자극제 상태에서 FP 1배·TP 2배를 적용한다. 효과 전체를 0으로 만드는 구현은 원본 동작과 다르다.

`PSYCHO_TIMER`와 `CLOSE_DISTORTION`은 함께 이식해야 한다. 전자는 호출될 때 카운터를 2로 갱신하고 후자는 감소시킨 뒤 효과를 판정한다. 이를 임의의 초 단위 타이머로 바꾸면 호출 순서에 따른 원본 동작을 잃는다. 후속 명령이 전체 XMM 값을 복사하므로 하위 실수값만 확인하는 테스트도 충분하지 않다.

근거: CT 9665–9759. 기존 시점 입력을 재사용할 수 있어 FOV나 HUD 전체 구현을 기다릴 필요는 없다.

### NAH 조준점과 붉은 빈사 효과

원본의 NAH 조준점 보정은 해당 DLC·TP·조준 중이라는 조건에서만 적용한다. 게임의 조준점 설정이 조준할 때만 표시인 경우를 포함해 확인해야 한다. HUD 전체 숨김과 분리해 먼저 적용할 수 있다.

NAH의 붉게 맥동하는 빈사 효과는 기본 FP 유지·TP 억제다. 이 지점은 다음 명령이 사용하는 비교 결과를 바꾸므로 화면 값만 바꾸는 것으로 끝나지 않는다. FP·TP별 분기 결과까지 대조해야 한다.

근거: CT 4007–4030, 9629–9662, 기본 설정 27694–27716. 이 두 기능은 아래의 일반 빈사 셰이더와 별개의 처리다.

## 3 함께 연결해야 하는 상태 보정

### 혈액 효과

원본 기본 임계값은 FP 100%·TP 0%다. 퍼즐 등의 `stoppedbloodhide` 예외를 먼저 처리하고, 유효한 체력 주소가 없으면 게임의 원래 값을 사용한다. 현재 Native에는 이 화면 효과에 필요한 체력 주소 생산이 없다.

따라서 혈액 효과 훅과 함께 기존 `CUTSCENE_AUTOLOOK`의 체력 수집 경로를 보완해야 한다. 원본은 플레이어 체력을 10으로 나눈 값과 임계값의 비트 표현을 비교한다. 임의의 정수 반올림이나 일반적인 부동소수점 비교로 대체하지 말고 원본 계산과 대조할 필요가 있다. 체력 주소 교체·로드·읽기 실패 시에는 과거 체력으로 효과를 판단하지 않아야 한다.

근거: CT 1213–1217, 9225–9271.

### 빈사 셰이더

`HURT_FILTER_READ`는 기본 TP 필터 2·FP 필터 0을 선택한다. 동시에 뒤따르는 분기가 실행되지 않도록 비교 결과를 강제한다. 이 처리는 빈사 분기 외의 원래 값 경로에도 존재한다. 필터 번호만 바꾸고 게임의 기존 비교 결과를 유지하면 원본과 달라진다.

사용자 지정 필터가 기본 OFF여도 이 기본 빈사 보정은 유효하다. 반면 `FLASHBACK_FILTER_READ` 2개는 기본 OFF 상태에서 게임 값과 원래 비교를 유지하고 필터 상태를 추적한다. 따라서 일반 빈사 보정을 먼저 적용하고, 사용자 지정 필터와 그 2개 지점은 별도 선택 기능으로 남길 수 있다.

검사는 빈사·정상·미사용 값, FP·TP, 비교 플래그와 레지스터, 상태 종료 순서를 포함해야 한다. 근거: CT 10048–10171.

### 머리 숨김 지연과 Mia 머리카락 옵션

원본은 `stoppedbloodhide=1` 동안 지연값을 2로 갱신하고 이후 타이밍 훅 호출에서 감소시킨다. HEAD 처리에서는 FP이거나 지연값이 남아 있을 때 머리 숨김 경로로 들어간다. Native는 현재 FP일 때만 숨기고 TP 전환 시 머리 대상 캐시를 지우므로 카운터 하나를 추가하는 것으로는 복원되지 않는다.

허용 조건·대상 갱신·캐시 해제 시점을 함께 수정해야 한다. 전환 종료 시 머리가 돌아오는지, 로드나 캐릭터 교체 뒤 옛 대상에 쓰지 않는지, F6 카메라 비활성 상태의 기존 머리 숨김이 유지되는지 확인해야 한다. 실제 전환 화면의 차이는 게임에서 확인할 항목이다.

Mia 머리카락 처리는 이미 존재하지만 독립 토글이 없다. 원본은 AfroJackX의 현재 Mia 모델용 옵션을 기본 ON으로 두고 다른 모델에서는 끌 수 있도록 설명한다. 별도 토글을 복원하면 머리 숨김은 유지하면서 해당 머리카락 경로만 끌 수 있다. 기본값은 ON을 유지하는 편이 원본과 맞는다.

근거: CT 1383–1447, 3271–3279, 27749–27756. [현재 FP 제한](https://github.com/like-shock/re7-ots/blob/c21d8ed75c759471fffa83d20b2376b3da407378/diagnostics/src/native_runtime.c#L225), [TP에서 캐시 해제](https://github.com/like-shock/re7-ots/blob/c21d8ed75c759471fffa83d20b2376b3da407378/diagnostics/src/native_runtime.c#L275), [Mia 머리카락 선택](https://github.com/like-shock/re7-ots/blob/c21d8ed75c759471fffa83d20b2376b3da407378/diagnostics/src/native_head.c#L101).

분기 해석도 확인했다. Cheat Engine의 `@f`는 이름 있는 라벨을 포함한 다음 라벨을 가리킨다. 따라서 HEAD의 CT 1385·1387은 `skipheadthirdcheck`, 혈액 효과의 9257은 `cancelblood`, FOV 참조의 4156은 `code10`으로 이어진다. 가까운 `@@:`가 없다는 이유로 원문 오류라고 판단할 근거는 없다. [공식 Auto Assembler 구현](https://github.com/cheat-engine/cheat-engine/blob/master/Cheat%20Engine/autoassembler.pas#L782)

## 4 NAH HUD와 FOV의 적용 조건

### NAH HUD

`NAH_OXYGEN_REF`·`NAH_HUD_WRITE`·`NAH_OXYGEN_HUD_SWITCH`를 함께 구현해야 한다. 앞서 권장한 조준점은 이 3개 지점과 분리할 수 있다.

원본 HUD 처리는 단순히 TP 화면을 숨기는 코드가 아니다. 산소 감소, 오염 상태, 통신, 필터·야간투시·RAMROD 안내, 폭탄과 이벤트 진행, 처음 무기를 들지 않은 상태의 표시를 함께 다룬다. 특히 원본은 산소가 100인 경우를 확인하며, 산소가 줄어드는 동안에는 표시를 유지한다. 산소 전환 쓰기만 차단하거나 HUD를 무조건 숨기면 필수 정보와 갱신 동작이 달라질 수 있다.

구현 시 현재 표시 상태 주소·산소량·통신·이벤트 상태의 생산과 초기화를 함께 다뤄야 한다. 대체되는 원래 명령에는 게임 스택에서 R12를 읽는 명령도 있으므로, 현재 브리지에 원래 스택을 읽는 검증된 인터페이스를 마련하거나 동등한 원문 재실행 방식을 설계해야 한다. HUD 값만 쓰고 나머지 명령 효과를 생략하면 안 된다.

실기 확인은 산소 감소·회복, 통신 시작·종료, 필터 교체, 폭탄 대화, 재시작을 포함한다. 근거: CT 9274–9627, 10229–10244. [현재 브리지 프레임 정의](https://github.com/like-shock/re7-ots/blob/c21d8ed75c759471fffa83d20b2376b3da407378/diagnostics/src/native_bridge.h#L13).

### 원본 FOV

현재 `TRUE_FOV_READ`는 거리·focus 갱신에 연결돼 있지만 원본 FOV 정책은 포함하지 않는다. 원본 기본 TP 조준은 카메라 접근을 사용하면서 게임의 조준 FOV 비율을 별도로 반영하지 않고, 달리기 FOV 증가도 TP 기본 ON이다. Native에서 게임 FOV를 유지한 채 카메라만 접근시키면 원본보다 확대감이 강할 가능성이 있다. 이는 코드상의 차이이며 게임에서 확대율을 실측한 결론은 아니다.

필요한 묶음은 메뉴 값 수집, 원래 FOV 참조, REFramework 쓰기 감지, 본편·NAH·EOZ의 경쟁 쓰기 제어, 기존 FOV 계산 확장이다. REFramework가 최근 FOV를 쓴 경우 원본이 양보하는 동작도 보존해야 한다. 본편과 DLC 쓰기 3개만 차단하는 것은 독립적인 개선이 아니다.

기본 적용에서는 원본처럼 사용자 지정 FOV 강제를 OFF로 두고 게임 메뉴를 기준으로 삼을 수 있다. 원본의 기본 달리기 최대 증가는 20%이며, 기존 코드의 `runningfov` 카메라 거리 옵션과는 다른 기능이다.

검사는 본편·NAH·EOZ별 FP·TP 조준·달리기·컷신·메뉴 변경, REFramework FOV 사용 여부, F6 종료 후 게임 제어 복귀를 포함해야 한다. 근거: CT 2474–2853, 3764–3777, 4151–4188. [현재 FOV 연결](https://github.com/like-shock/re7-ots/blob/c21d8ed75c759471fffa83d20b2376b3da407378/diagnostics/src/native_runtime.c#L298).

## 5 선택 기능과 설정

| 대상 | 적용 판단 |
| --- | --- |
| XInput 2개 지점 | 버튼·방향 상태를 모드에 전달하는 원본 경로다. Freecam·배속 등 소비 기능과 사용자 조작 방식을 정한 뒤 적용한다. 이 누락은 게임 자체의 컨트롤러 조작 불가를 뜻하지 않으며 기존 Steam Input의 F6·F7 매핑과 별개다. |
| 사용자 지정 화면 필터 2개 지점 | 강제 필터는 원본 기본 OFF다. 필터 선택 기능을 제공할 때 일반 빈사 상태와 함께 연결한다. |
| Fullbright 7개 지점 | 선택형 조명 기능이다. 기본 OTS에 일괄 포함하지 않는 편이 적절하며, RELit 현상 해결책으로 검증된 것은 아니다. |
| 카메라·전환 설정 UI | 거리·높이·focus·충돌·자동 전환 등은 계산 구현과 사용자 선택 가능 여부를 나눠 평가해야 한다. Mia 머리카락 독립 토글을 먼저 검토하고 나머지는 실제 조정 요구에 맞춰 노출한다. |
| 기타 독립 스크립트 | Freecam·카메라 잠금·시간 배속·캐릭터 정지·무적·탄약 등은 별도 기능이다. 미구현 사실만으로 기본 OTS 결함에 포함하지 않는다. |

이전 검토에서 `XINPUT_FLOATS`를 축·트리거 입력으로 요약했던 부분은 정정한다. 해당 훅은 여러 입력 값을 검사해 모드용 버튼 상태 바이트를 채우며, 이름만 보고 아날로그 값을 그대로 전달한다고 볼 수 없다. 원본 소비자는 시간 배속 키, 아이템 상자 입력, Freecam 등의 별도 기능에 존재한다. 근거: CT 3505–3605, 3829–3960, 11975–12045 및 `xinputkey` 참조.

## 6 미연결 지점 전체 목록

선택된 원본 지점 78개 중 **51개 연결·27개 미연결**이다. Fullbright를 제외하면 71개 중 51개 연결·20개 미연결이다. 한 지점에 여러 기능이 있으므로 기능 완성률로 환산하면 안 된다. Critter·머리 지연·설정 선택 같은 부분 이식은 이 미연결 숫자에 포함되지 않는다.

| 원본 블록 | 전체 | 연결 | 미연결 |
| --- | ---: | ---: | ---: |
| Main | 42 | 33 | 9 |
| Camera Aim Collision | 7 | 7 | 0 |
| Auto Disable | 3 | 3 | 0 |
| Other | 19 | 8 | 11 |
| Fullbright | 7 | 0 | 7 |

원본 전체 CT는 320개 항목·72개 스크립트다. 아래 78개 대상 목록은 저장소가 선택한 패치 범위이며 CT 전체 기능 목록은 아니다. [원본 추출 목록](https://github.com/like-shock/re7-ots/blob/c21d8ed75c759471fffa83d20b2376b3da407378/diagnostics/data/patch_manifest.json)

| 원본 블록 | 지점 | CT 시작 행 | 적용 분류 |
| --- | --- | ---: | --- |
| main | `MENU_FOV_BYTE` | 2474 | 4번 FOV 묶음 |
| main | `REFRAMEWORK_FOV_WRITE_POINT` | 3764 | 4번 FOV 묶음 |
| main | `XINPUT_WRITE` | 3829 | 선택형 입력 확장 |
| main | `XINPUT_FLOATS` | 3880 | 선택형 입력 확장 |
| main | `NAH_CRIT_HP_EFFECT_CMP` | 4007 | 1번 NAH 빈사 효과 |
| main | `TRUE_FOV_REFERENCE` | 4151 | 4번 FOV 묶음 |
| main | `FOV_WRITE_MAIN` | 4173 | 4번 FOV 묶음 |
| main | `FOV_WRITE_NAH` | 4179 | 4번 FOV 묶음 |
| main | `FOV_WRITE_EOZ` | 4185 | 4번 FOV 묶음 |
| other | `SCREEN_BLOOD_WRITE_FOLLOW` | 9225 | 2번 체력 수집과 함께 |
| other | `NAH_OXYGEN_REF` | 9274 | 3번 HUD 묶음 |
| other | `NAH_HUD_WRITE` | 9291 | 3번 HUD 묶음 |
| other | `NAH_DOT_FORCE` | 9629 | 1번 조준점 |
| other | `VIGNETTE` | 9665 | 1번 기본 보정 |
| other | `PSYCHO_TIMER` | 9692 | 1번 색수차와 함께 |
| other | `CLOSE_DISTORTION` | 9709 | 1번 상태 생산자와 함께 |
| other | `HURT_FILTER_READ` | 10048 | 2번 빈사 셰이더 |
| other | `FLASHBACK_FILTER_READ` | 10116 | 선택형 사용자 지정 필터 |
| other | `FLASHBACK_FILTER_READ_2` | 10145 | 선택형 사용자 지정 필터 |
| other | `NAH_OXYGEN_HUD_SWITCH` | 10229 | 3번 HUD 묶음 |
| fullbright | `LIGHT_SOURCE_DIAMETER_READ` | 15622 | 선택형 Fullbright |
| fullbright | `LIGHT_RADIUS_MULSS_READ` | 15736 | 선택형 Fullbright |
| fullbright | `LIGHT_RADIUS_MOVSS_READ` | 15760 | 선택형 Fullbright |
| fullbright | `EXAMINE_LIGHT_CMP_1` | 15784 | 선택형 Fullbright |
| fullbright | `EXAMINE_LIGHT_CMP_2` | 15815 | 선택형 Fullbright |
| fullbright | `GLOBAL_BRIGHTNESS` | 15846 | 선택형 Fullbright |
| fullbright | `G_BRIGHTNESS_2` | 15875 | 선택형 Fullbright |

## 7 검증 결과와 다음 구현 기준

원본 해시와 지점 목록을 다시 확인했고 0.1.5에서 Critter 생산 누락을 재현했다. 임시 검사는 게임 메모리를 모의 처리한 macOS x86_64 호스트 검사이며 게임 실행 검사가 아니다. 제품 저장소에는 추가 변경이 없다.

기존 0.1.5는 [GitHub Actions 빌드](https://github.com/like-shock/re7-ots/actions/runs/39209973997)가 성공한 상태다. 아직 구현하지 않은 위 후보들이 그 빌드에서 검증됐다는 뜻은 아니다. 다음 구현도 GitHub Actions에서 DLL·패키징과 원본 대조 검사를 수행하고, 화면 동작은 Steam Deck에서 확인하는 방식이 적절하다.

추천하는 바로 다음 범위는 **1번 묶음**이다. 새 지점 5개는 `VIGNETTE`, `PSYCHO_TIMER`, `CLOSE_DISTORTION`, `NAH_DOT_FORCE`, `NAH_CRIT_HP_EFFECT_CMP`이며, 기존 아이템 훅의 Critter 판별을 함께 보완한다.
