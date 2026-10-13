# 상태 관찰기 v0.2.0 설계

근거: ../HANDOFF.md 5절 및 사용자의 “계획대로 개발” 요청. 기존 진단기의 제한된 확장이다.
게임을 바꾸지 않고 실제 조작에 따른 후보 값과 재획득을 기록한다. 실제 3인칭, DLC, Fullbright는 포함하지 않는다.

- C portable observer: CT L1182–1212의 player/aim/inventory 체인과 RVA 0x8FB17F0의 원시 슬롯. 모든 읽기는 주입된 reader를 통해 수행한다. Windows에서는 ReadProcessMemory 성공과 전체 길이를 확인한다. 각 샘플마다 루트부터 재획득하고 읽은 링크를 역순 재확인한다. NULL/읽기 실패/비정상 포인터/변경 감지/비정상 값/빌드 불일치를 구분한다. 이것은 원자적 스냅샷이나 객체 수명 보장이 아니다.
- Windows wrapper: 기존 core.c의 PE/SHA 검사와 Win32/REF prefix ABI 재사용. 엔진 함수는 호출하지 않는다. 고정 체인은 알려진 실행 파일 SHA-256과 메모리 PE 식별값이 일치할 때만 읽는다. F8 시작/종료, F7 단계 표시. 별도 로컬 worker가 최대 10Hz, 최대 15분/9,000샘플/16MiB, 실행당 4세션만 기록한다. 키는 게임 전경에서만 처리. 로그 I/O는 worker. 전역 공유값은 atomic 접근.
- Lua: 공식 sdk.get_primary_camera() → runtime-verified get_Transform → documented get_position/get_rotation. UpdateBehavior 후 callback에서만 엔진 조회, 최대 10Hz. 숫자와 객체 주소 식별값만 보존하고 객체 자체는 다음 샘플에 캐시하지 않는다. 현재 native 세션/seq/stage와 카메라를 연계하되 동시 취득으로 표현하지 않는다. stale/partial status와 API 미지원은 UNKNOWN. Lua의 reference-count 관리까지 없다고 주장하지 않는다. 직접 setter/patch/hook 없음.
- Native와 camera는 같은 세션 이름의 두 JSONL 파일. 자체 로그/상태 파일만 작성한다. UI에는 획득 가능 여부와 오류를 표시하며 의미 검증 완료라고 하지 않는다. F7 단계는 사용자가 붙인 라벨이지 행동 감지가 아니다.
- 기존 v0.1.0은 소스/테스트 보존. 설치 시 이전 진단 DLL/Lua 두 개만 비활성화하도록 안내한다. REFramework/RELit/Proton은 유지.

## 검증 경계
모의 메모리, 실제 portable C, Windows wrapper 가짜 Win32 backend, Lua 실제 런타임과 SDK stub으로 시험한다. 실제 엔진 객체 및 callback 시점의 안전성, Steam Deck 로딩/호환성/성능은 사용자 실기에서 확인한다. pcall은 native crash를 완전히 보호하지 않는다.
