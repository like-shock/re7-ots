# Ethan with Head RT 분석 및 연결 근거

분석일: 2026-10-13. 사용자가 제공한 `Ethan with Head RT-86-1-0-1669695232.rar`를
별도 작업 폴더에서 읽었다. 제작자 메타데이터: AfroJackX, Ethan with Head Model RT 1.0,
3인칭 스크립트용 정적 머리 모델. 모델·텍스처·미리보기 이미지는 저장소에 포함하지 않는다.

RAR SHA-256: `231bb2c129eb52eff7d78a1ea09ae456bbdd1646eba24d8504d3549ec2687cfb`.
파일 6개, 압축 해제 크기 6,309,163바이트. 아래 경로의 공통 접두사는
`natives/stm/character/player/pl0000/`이다.

| 리소스 | 크기 | SHA-256 |
| --- | ---: | --- |
| `pl0001/pl0001.mesh.220128762` | 3,456,800 | `010ca6976245f470c2b7c90a4f06b572c08fe59f82fabcb57d90d7a163e8ea18` |
| `pl0001/pl0001.mdf2.21` | 7,664 | `d177e83e6fbb4d21967b327909e86ed8e9a5a8bb01438c2a6dd0506c398f4d4a` |
| `pl0002/pl0002.mesh.220128762` | 2,175,856 | `2c9b20d046e12a8197277cce6b3ed960887ee0ab3f67f87bbaedd9922f658a0c` |
| `pl0002/pl0002.mdf2.21` | 7,992 | `a8357382c2cb8f6db098f40101de02e12144839df1791b7948e9b92199fcb924` |

## 메시와 관절

두 메시의 내부 MESH 버전은 21041600이며 파일 헤더의 크기와 실제 크기가 일치한다.

| 항목 | pl0001 | pl0002 |
| --- | --- | --- |
| 역할 | 몸체 | 얼굴·눈·머리카락 |
| 관절 수 | 158 | 253 |
| LOD 수 | 1 | 1 |
| 재질 | `pl0000_body`, `pl0000_L_Leg`, `pl0000_VR` | 아래 5개 |
| Head 영향 | Head 하위 관절 가중치 없음 | 얼굴을 제외한 네 부분은 모두 Head 하위에 연결 |

머리 메시 재질은 `pl0050_face`, `pl0050_eye`, `pl0050_hair`, `pl0050_eye_tear`,
`pl0050_eye_shadow`다. 얼굴 정점 9,800개 중 8,816개는 Head 하위 관절에만 연결되고,
528개는 그 바깥 관절의 영향도 받으며, 456개는 Head 하위 관절의 영향을 받지 않는다.
바깥 관절은 Neck/Chest다. **Head 관절만 이동하면 얼굴 일부가 남거나 늘어날 수 있다.**
따라서 분리된 `pl0002` 메시의 GameObject `DrawSelf`를 제어하는 방식을 선택했다.
몸체의 Neck/Chest를 옮기거나 전체 플레이어를 숨기지 않는다.

메시 분석에는 [RE-Mesh-Editor의 고정 커밋](https://github.com/NSACloud/RE-Mesh-Editor/tree/622daa75b41ec622c3444ecb44ef31a651692687)을
별도 작업 폴더에서 사용했다. 분석 결과의 수치·경로만 기록했으며 GPL 파서 코드를
이 MIT 프로젝트로 복사하지 않았다. 저장소의 패키지 검사 도구는 메시 파서가 아니다.

## 런타임 연결 근거와 제한

- 로컬 플레이어: REFramework [RE7.lua](https://github.com/praydog/REFramework/blob/d052e8fd3fd41d0a1a33394fcf6e6c19be39b1ee/scripts/utility/RE7.lua)의 `app.ObjectManager.PlayerObj`.
- 관절 소유자·월드 위치: [RETransform.cpp](https://github.com/praydog/REFramework/blob/d052e8fd3fd41d0a1a33394fcf6e6c19be39b1ee/shared/sdk/RETransform.cpp)의 `getJointByName`, `get_Owner`, `get_Position`.
- 자식 순회·메시 경로·재질·DrawSelf: [EMV-Engine 고정 커밋](https://github.com/alphazolam/EMV-Engine/tree/a8a89961fde640d542afc72bd66fd8e12f3b82b3)의
  `EMV Engine/reframework/autorun/EMV Engine/init.lua`에서 해당 REFramework 호출 사용을 확인했다.
  `get_Child/get_Next/get_Parent`, `get_Components`, `getMesh`, 리소스 `ToString()`,
  `get_MaterialNum/getMaterialName`, `get_DrawSelf/set_DrawSelf`다.

바인딩은 호출 전에 메서드를 확인한다. 로컬 플레이어 하위에서 정확한 경로와 재질 5개가
맞는 머리 메시 하나만 허용한다. 해당 GameObject에 다른 Mesh가 있거나 후보가 여러 개면
대기한다. Head 관절의 소유자도 해당 Transform과 일치해야 한다.

128개 노드/깊이 12의 한도 안에서 **매 조회마다 다시 순회**한다. 경로만 캐시하면 나중에
추가된 중복 후보를 놓칠 수 있어 이번 버전에서는 캐시를 사용하지 않았다. 프레임 사이에는
관리 객체를 보관하지 않으며, 쓰기 직전에도 대상 식별자와 기존 값을 비교한다.
실기에서의 비용은 아직 측정하지 않았다.

네이티브 세션·플레이어 세대와 객체 주소를 함께 사용하지만, 상태 샘플 사이에 모든 주소가
재사용되는 상황을 완전히 식별하는 엔진 수명 토큰은 없다. 입력이 오래되면 기존 대상 숨김만
시도하며 새 대상 획득·표시는 중단한다. 네이티브 세대가 바뀌면 이전 복원값을 폐기한다.

이 자료는 API 사용 근거다. 실제 RE7에서 설치한 머리 메시가 예상한 객체 구조로 나타나는지,
DrawSelf만으로 최종 화면을 숨길 수 있는지는 아직 실기로 확인하지 않았다. EMV는 상황에 따라
다른 갱신·그리기 플래그도 사용하지만, 이번 코드는 해당 원시 메모리 쓰기를 가져오지 않는다.
게임이 DrawSelf를 다시 바꾸면 소유권 충돌을 기록하며 표시 재시도를 중단한다.
