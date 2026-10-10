# 작업 지시: OSD의 전체 화면·비율 유지 옵션과 `cfg/repiu.ini` 저장 (issue #45)

설계: `docs/design/20261010-i045-osd-fullscreen-aspect.md`

1. `glide_letterbox`: `ComputeGlidePictureRect(logical, drawable, keep_aspect)`. letterbox probe에 늘림
   사각형을 더하고 core probe에도 넣는다.
2. `repiu/engine/session_display_preferences.h/.cpp`: 세션 sink 등록·해제·호출.
3. 백엔드: `keep_aspect_`·전체 화면 요청의 초기값(환경 변수), 시작 시 전체 화면 진입,
   `ApplyDrawableViewport`가 `ComputeGlidePictureRect`를 씀, 이벤트 펌프 시작에서 OSD 요청 적용,
   사용자 조작으로 바뀌었을 때만 sink 호출(Alt+Enter·더블 클릭 포함).
4. OSD: "Fullscreen", "Keep aspect ratio" 체크박스와 툴팁.
5. 런처 설정: `[Video] fullscreen`, `keep_aspect` 읽기·쓰기·경고, 환경 변수 게시와 우선순위, 런처
   Options 체크박스, `SettingsDiffer`.
6. 로더: 게시 로그, 게임 실행 전 sink 등록(파일을 다시 읽어 두 키만 바꿔 저장).
7. `scripts/survey_romsets.sh`: `REPIU_GLIDE_FULLSCREEN=0` 기본.
8. 문서: ARCHITECTURE, README, 관련 가이드(설정 파일), 작업 로그.
9. 검증: Linux x64·i386 Release 빌드, core probe, 사용자 화면에서 OSD·Alt+Enter·재실행.

완료 기준: OSD와 런처에서 두 옵션을 바꿀 수 있고, 비율 유지를 끄면 그림이 창 전체를 채우며, 사용자
조작으로 바뀐 값이 `cfg/repiu.ini`에 저장되어 다음 실행이 그 상태로 시작한다. 환경 변수는 여전히
파일보다 우선한다.

---

# Work order: OSD fullscreen and keep-aspect options stored in `cfg/repiu.ini` (issue #45)

(1) `glide_letterbox`: `ComputeGlidePictureRect(logical, drawable, keep_aspect)`; the letterbox probe
gains a stretch rectangle and joins the core probe. (2) `repiu/engine/session_display_preferences`:
register, clear and call a session sink. (3) Backend: initial `keep_aspect_` and fullscreen request
from the environment, entering fullscreen at start, `ApplyDrawableViewport` using
`ComputeGlidePictureRect`, OSD requests applied at the start of the event pump, and the sink called only
when a user action changed a value (Alt+Enter and double click included). (4) OSD: "Fullscreen" and
"Keep aspect ratio" checkboxes with tooltips. (5) Launcher settings: read, write and warn for
`[Video] fullscreen` and `keep_aspect`, publish them under the environment precedence rule, add the
launcher checkboxes and extend `SettingsDiffer`. (6) Loader: the publish log line, and the sink
registered before the game runs (re-read the file, change the two keys, write it back). (7)
`scripts/survey_romsets.sh` defaults `REPIU_GLIDE_FULLSCREEN=0`. (8) Docs: ARCHITECTURE, README, the
settings guide, the work log. (9) Verify: Linux x64 and i386 Release builds, the core probe, and the
OSD, Alt+Enter and a rerun on the user's screen.

Done when both options can be changed from the OSD and the launcher, keep aspect off fills the window,
a value changed by a user action is stored in `cfg/repiu.ini` and the next run starts that way, and the
environment variables still win over the file.
