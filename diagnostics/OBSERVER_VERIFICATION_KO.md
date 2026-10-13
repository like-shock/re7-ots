# RE7 OTS Observer v0.2.1 검증 결과

2026-10-13 후속 수정: native DLL은 v0.2.0 유지, Lua 카메라 조회기만 v0.2.1입니다. 카메라에서 소유 GameObject를 거쳐 Transform을 얻도록 수정했습니다. 실기 로그 분석 및 교체 안내는 `OBSERVER_CAMERA_FIX_KO.md`에 있습니다.

- 사용자 v0.2.0 실기 로그: native 1,451개, camera 1,129개. 모두 정상 종료 레코드. 카메라는 전부 UNKNOWN_TRANSFORM_METHOD.
- 올바른 객체 계층으로 모의 객체를 바꾸고 새 회귀 테스트 3개를 추가. 수정 전 4개 테스트 실패, 수정 후 전체 28개 통과.
- C 핵심 검사 30개, 관찰 코어 12개 시나리오 통과. 재빌드한 DLL은 아래 v0.2.0 해시와 동일.
- 독립 검토자가 변경 사항을 검토하고 전체 검사를 다시 실행. 새 문제 보고 없음.
- v0.2.1 카메라 경로의 실제 Steam Deck 재검증은 아직 필요합니다. 아래의 v0.2.0 최초 배포 기록에서 실기 미검증이라고 적은 부분은 당시 상태입니다.

이번 증거: `evidence/v0.2.1/red_camera_owner.txt`, `final_tests.txt`, `user_trace_summary.json`.

## 보관: v0.2.0 최초 배포 당시 검증

검증일: 2026-10-13. 대상: 현재 인계 자료에서 개발한 상태 관찰기.
**개발·빌드·모의 검증 완료, 실제 RE7/Steam Deck 실기 미검증**입니다.

## 실행한 검증

| 항목 | 결과 | 범위 |
|---|---|---|
| 기존 C 핵심 검사 | 30개 통과 | 기존 PE/패턴/SHA 등 |
| 새 C 관찰 코어 | 12개 시나리오 통과 | 정확한 체인, 읽기 실패, NULL, 잘못된 포인터, 읽기 중 변경, NaN, 비정상 플래그, 재획득, fixture 비변경 |
| Python unittest 전체 | 25개 통과, 실패·건너뜀 0 | 기존 18개 + 새 DLL 검사·native host·Lua 5개 회귀 테스트 |
| Native wrapper 모의 OS 실행 | 12개 모드 통과 | 정상, 해시 불일치, 읽기/쓰기 오류, 짧은 쓰기, 시간/개수/크기 제한, thread 실패, 전경 제한, 검증 중 취소, 세션 상한 |
| Lua 실행 | 실제 liblua5.4 + SDK/I/O stub | 카메라 없음·교체·예외, 메서드 미지원, 비정상 값, stale/partial 상태, 중복 샘플 억제, 파일 오류 |
| Windows x64 DLL | clang/lld 18.1.3 빌드 성공 | PE x64, 필요한 export 2개, unwind/relocation, KERNEL32/USER32만 의존 |
| 메모리 쓰기 관련 import | 금지 import 없음 | WriteProcessMemory, VirtualProtect, 원격 thread, 네트워크 계열 등 검사. 이것만으로 모든 실행 안전성을 증명하지 않음 |
| 반복 빌드 | DLL·Lua SHA 동일 | 동일 소스/툴체인으로 재빌드 |
| 빌드 결과 없는 소스 복사본 | 전체 25개 재통과, DLL 일치 | 기존 build/ 없이 test.sh 실행 |
| 설치 ZIP | CRC 및 DLL/Lua 바이트 일치 | 검사한 결과물과 ZIP 내용 대조 |
| ASan/UBSan | C 코어 및 모의 worker 정상/9,000샘플 제한 시험 통과 | 호스트 Linux 실행. Windows DLL 실행 검사가 아님 |

Python 25개 안에 native 12모드와 여러 Lua 시나리오가 포함되어 있습니다. 숫자를 모두 더해 독립 테스트 수로 표현하지 않습니다.

## 독립 코드 검토와 수정

별도 검토자가 전체 코드를 검토하고 당시 21개 unittest와 C 검사를 실행했습니다. Critical 문제는 보고하지 않았고 다음 두 문제를 찾았습니다.

1. native 상태가 끊기거나 JSON이 잘못되면 Lua가 제한 시간 확인 전에 반환하여 카메라 파일이 열린 채 남을 수 있음.
2. callback API 부재/등록 실패 원인이 일반 상태 갱신에 덮여 화면에서 사라짐.

각 상황을 재현하는 **Lua 회귀 테스트 4개가 수정 전 실패한 것을 확인**한 뒤 수정했습니다.
현재는 프레임 폴링에서 상태 파일 해석보다 먼저 종료 제한을 검사하며, callback 실패 이유를 별도 유지합니다.
오래된 native 값은 UNKNOWN, 카메라 기록 대기는 PAUSED_NATIVE_UNKNOWN으로 표시합니다.
검토자 수정 재검토를 별도로 수행한 것은 아니며, 수정 후 작성자가 전체 25개 테스트를 재실행해 통과를 확인했습니다.

## 확인하지 못한 항목

- 실제 Steam Deck/SteamOS/Proton에서 DLL 로딩, callback 실행, 파일 경로 및 파일 동작.
- 실제 플레이어·조준·인벤토리 값의 의미와 조작 대응.
- 실제 재로딩 후 객체 재획득, 게임 객체 수명 및 callback 시점 안전성.
- CT 카메라 대상과 REFramework primary camera transform의 동일성.
- 실제 게임에서의 오버헤드, RELit 병용, 장면 전환과 특수 연출.
- LSan 누수 검사: 환경에서 `/proc` task 조회가 차단되어 실행 불가. ASan 실행 시 `detect_leaks=0`을 명시했으며 누수 검사 통과로 계산하지 않음.
- Native mock은 OS API와 알려진 해시 출력 일부를 대체합니다. Lua mock은 SDK와 JSON 직렬화/I/O를 대체합니다. 모의 환경이 엔진 안전성을 보장하지 않습니다.

게임은 관찰 중에도 변경됩니다. 링크 재확인은 일부 경합을 감지하지만 원자적 스냅샷·주소 재사용(ABA)·잘못된 객체의 의미를 검증하지 않습니다. Lua pcall은 모든 native crash를 막는 장치가 아닙니다.

## 빌드 해시

```text
RE7_OTS_Observer.dll
8230932edbc625fc1b455aab52274b67b4ad115cee908e14e21f93a4e1abb2a2

re7_ots_observer.lua
e82d153dc260e11e5b15539d84a96bec48097444c62268c6a66849ac885a1dd5
```

재현 명령: `OBSERVER_BUILD.md`. 실행 출력: `evidence/v0.2.0/final_tests.txt`, `red_review_fixes.txt`, `asan_ubsan.txt`, `build_hash_first.txt`, `build_hash_second.txt`.
이전 `evidence/` 루트의 파일은 v0.1.0 당시 기록이며 이번 결과와 분리했습니다.
