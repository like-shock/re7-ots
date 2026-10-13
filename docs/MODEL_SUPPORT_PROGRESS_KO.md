# 머리·모델 지원 진행 상황

2026-10-13. `work` 브랜치의 **게임 연결 전 기반 구현**이다. 머리가 게임에 표시되는 상태까지
완료한 것은 아니다. Explore v0.3.0 카메라/DLL/배포 Lua와 중단 조건은 그대로 유지한다.

## 추가한 코드

### 모델 패키지 검사

`diagnostics/tools/inspect_model_package.py`는 모델 ZIP 또는 압축 해제된 폴더를 읽어
상대 경로, 크기, SHA-256, 리소스 종류와 파일 이름의 숫자 접미사를 JSON으로 출력한다.
파일을 설치하거나 수정하지 않는다. 경로 충돌, 중복, 손상된 ZIP, 링크와 크기 제한 초과는 오류로 처리한다.

```bash
python3 diagnostics/tools/inspect_model_package.py /path/to/model.zip > /tmp/model-inventory.json
# RAR/7z는 먼저 별도 폴더에 압축을 해제한 뒤 해당 폴더를 지정한다.
python3 diagnostics/tools/inspect_model_package.py /path/to/extracted-model > /tmp/model-inventory.json
```

출력 파일은 검사하는 폴더 밖에 둔다. 기본 제한은 항목 4,096개(경로에 암시된 폴더 포함),
전체 파일 내용 256 MiB, ZIP 자체 512 MiB, 경로 4,096 UTF-8 바이트/64단계다.
보고서에는 파일 내용이나 호스트의 절대 경로가 들어가지 않는다.
경로·숫자 접미사만으로 RT 호환성, Ethan 모델, 머리 분리 여부를 판정하지 않으므로
`compatibility`는 항상 `UNVERIFIED`다. 메시/관절의 바이너리 파서는 아직 없다.

### 표시 제어 수명주기

`diagnostics/lua/model/visibility.lua`는 외부 연결부를 주입받는 Lua 모듈이다.
SDK 호출, 객체 검색, 모델 파일이나 관절 이름을 포함하지 않는다.

| 호출 | 처리 |
| --- | --- |
| `before_camera_restore()` | 머리를 숨긴 뒤 호출자가 카메라를 복원하도록 요청 순서를 제공 |
| `after_camera_apply(true)` | 호출자가 실제 카메라 쓰기·읽기 확인과 머리 이격 거리를 확인한 뒤에만 표시 허용 |
| `after_camera_apply(false/nil/기타)` | 숨김 처리. F6 요청 여부만으로 표시하지 않음 |
| `release()` | 동일 대상에서 아직 소유한 상태만 최초 값으로 복원. 실패 시 재호출 가능 |

쓰기 후 값을 다시 읽으며, 예외 발생 뒤 값이 바뀐 경우에도 정리할 정보를 유지한다.
대상 교체나 외부 값 변경을 발견하면 이전 값을 덮어쓰지 않고 오류를 유지한다.
쓰기 직후 읽은 값이 요청값과 다르면 무시된 setter인지 외부 변경인지 구분할 수 없으므로
소유권을 내려놓는다. 이 경우 자동 복구는 시도하지 않으며 숨김 상태가 남을 수 있다.
대상 식별자가 바뀌면 새 연결부 검증 후 새 컨트롤러를 만들어야 한다.

**연결부 계약:** `read()`는 검증된 현재 로컬 플레이어의 머리 전용 대상에 대해
`{id=세대가 포함된 고유 문자열, visible=boolean}` 또는 대상 없음(`nil`)을 반환한다.
`write(id, expected, desired)`는 쓰기 직전에 대상을 다시 찾고 식별자와 현재 값을 비교한다.
성공 시 `true`, 실제 쓰기 전 거부 시 `false`를 반환한다. 엔진 setter가 실행된 뒤 오류가
나면 예외를 던져 읽기 확인과 복구가 가능하게 한다. 같은 주소가 재사용되어도 세대 식별자는
달라야 한다. 여러 관절 값으로 표시를 구현한다면 연결부가 각 값의 소유권·복원을 관리해야 한다.
단순한 boolean 결과로 실제 관절 값 검증을 대신할 수 없다.

머리 숨김 실패가 카메라 복원을 막아서는 안 된다. 연결부는 실패를 기록하면서 카메라를
복원하고, 다음 엔진 콜백에서 자신이 소유한 변경의 정리를 재시도해야 한다.
`release()`는 원래 상태 복원이며 1인칭 숨김과 다르다. 원래 표시된 자산이라면 해제 후
다시 표시될 수 있으므로, 게임 연결 단계에서 재설정/종료 시 처리까지 결정해야 한다.

이 모듈은 현재 자동 실행 번들에 들어가지 않는다. 따라서 조준·인벤토리·F6·리셋과의
실제 콜백 연결 및 렌더링 순서는 아직 구현/검증되지 않았다. 새 게임용 설치본도 만들지 않았다.

## 확인한 공식 인터페이스와 남은 조건

REFramework의 [`scripts/utility/RE7.lua`](https://github.com/praydog/REFramework/blob/d052e8fd3fd41d0a1a33394fcf6e6c19be39b1ee/scripts/utility/RE7.lua)는
`sdk.get_managed_singleton("app.ObjectManager")`의 `PlayerObj` 필드로 로컬 플레이어를 찾는다.
여기까지는 공식 소스 근거가 있지만, 그 객체가 Ethan인지와 외부 모델의 머리 대상은 별도 확인이 필요하다.

[`RETransform` 문서](https://refdocs.praydog.com/api/types/RETransform.html)와
[`Sdk.cpp` Lua 바인딩](https://github.com/praydog/REFramework/blob/d052e8fd3fd41d0a1a33394fcf6e6c19be39b1ee/src/mods/bindings/Sdk.cpp)를 확인했다.
공식 `FirstPerson.cpp`의 C++ 관절 행렬 처리도 확인했지만, 이를 RE7 Lua의 머리 표시 API로
간주하지 않았다. 사용 중인 REFramework 빌드에서 지원하는 실제 대상별 조작을 연결해야 한다.

다음으로 필요한 입력은 [Third Person Full Model Placeholders의 Files 페이지](https://www.nexusmods.com/residentevil7/mods/86?tab=files)의
**Ethan with Head RT**(Optional files, 2.7 MB) 또는 RT 본편 묶음이다.
구형 DX11용 `Ethan with Head`와 구분한다. 원본 압축 파일을 받으면 위 검사 도구로 내용을
확인하고, Ethan 리소스 경로와 머리/몸체 구성을 근거로 연결부를 정할 수 있다.
파일 이름이나 설명만으로 이 구조를 확정할 수 없어 지금은 런타임 연결을 보류했다.

모델 원본은 저장소에 재배포하지 않는다. 지금 Steam Deck을 실행하거나 조준 테스트를 할
필요는 없다. 모델 파일 분석 후에도 실제 객체 구조·렌더 순서는 통합 플레이에서 확인할 부분이다.

## 자동 검증

패키지 검사에는 임시 ZIP/폴더를, 표시 제어에는 실제 Lua 5.4 인터프리터와 가짜 연결부를 사용한다.
정상 전환, 대상 부재/교체, 외부 변경, 거부/무시된 쓰기, 변경 후 예외, 읽기 실패 및 정리 재시도를 검사한다.
이 검사는 제어 모듈과 파일 도구의 결과이며 실제 RE7 모델 표시 성공을 뜻하지 않는다.

독립 검토의 주요 2건(외부 변경값의 잘못된 복원, 깊은 ZIP 경로의 제한 우회)은 재현 검사 후 수정했다.
경미한 후속 항목은 손상된 LZMA ZIP의 오류 메시지 정리와, 명시적 폴더 항목이 없는 ZIP에서
부모 폴더 이름의 대소문자 충돌 판정이다. 손상된 파일은 정상 원본이 필요하며,
대소문자가 모호한 ZIP은 추가 확인이 필요하다.

```bash
python3 -m unittest discover -s diagnostics/tests -p 'test_model_*.py' -v
bash diagnostics/test.sh
```

전체 검사 결과와 구현 판단은 [구현 기록](superpowers/plans/2026-10-13-model-foundation.md)에 남긴다.
