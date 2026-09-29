# Task 761 작업 로그: Glide LFB 고정밀 표시 경로와 in-game OSD 토글

설계: `docs/design/20260929-761-glide-lfb-high-precision-and-osd.md`
지시: `docs/work-orders/20260929-761-glide-lfb-high-precision-and-osd.md`

## 구현 결과

1. `WriteGlideLfbRegionRgba8`를 `glide_lfb_region`에 추가했다. 클리핑·stride 판정은
   565 writer와 같은 규칙이고, 888/8888은 채널을 그대로 옮기며 16-bit 소스는
   `ConvertSourcePixelTo565` → decode 확장 왕복으로 off 경로와 동일한 값을 만든다.
2. `ThreadContext`에 `glide_lfb_region_rgba8_shadow`/`_valid`/`_present_count`를
   추가했다.
3. 경계의 세 지점을 배선했다: seed는 readback RGBA8을 encode 전에 shadow로 옮기고,
   write는 토글이 켜져 있을 때 병행 기록하며(invalid면 현재 565 staging의 decode로
   초기화), flush는 shadow가 유효하면 decode를 건너뛰고 그대로 present한다. 토글이
   꺼져 있으면 flush가 shadow를 invalid로 만들어 재활성화가 항상 신선한 decode에서
   시작한다.
4. `GlideOpenGlBackend`가 `std::atomic<bool>` 토글을 소유한다. 창이 열릴 때
   `REPIU_GLIDE_LFB_HIGH_PRECISION`을 `runtime::ResolvePromotedToggle`로 읽으므로
   미지정·빈 값은 on, `0|off|false`와 오타는 off다. 이것이 CLI 제어다(처음에는
   `ResolveOptInToggle`로 기본 off였다 — 아래 "후속 변경").
5. 새 `GlideOsd`(`include/repiu/engine/glide_osd.h`, `src/engine/glide_osd.cpp`)가
   인게임 OSD의 첫 구현이다. `Tab`이 열고 닫으며(처음에는 `F1` — 아래 "후속
   변경") Tab은 게임 입력에 전달되지 않고,
   OSD가 열려 있어도 다른 입력은 게임으로 계속 흐른다. 항목은 토글 체크박스
   하나로 backend의 atomic을 직접 읽고 쓴다. buffer swap 직전에 렌더되고 dummy
   mode에서는 만들지 않는다. CMake에서 `repiu_exe`가 `repiu_imgui`를 링크한다
   (웹 빌드 제외).

## 검증

* **Debug 빌드**: `scripts/build_win32_x86.ps1 -Configuration Debug`로
  `repiu_exe`/`repiu_aot_probe`/`repiu` 모두 exit 0.
* **Probe**: `glide_lfb_region_probe`에 `high_precision_shadow` 케이스를 추가했다 —
  8888 바이트 보존(`0x42`처럼 565로는 구분 불가한 값), 16-bit 소스의
  decode 왕복 일치, 완전 바깥 사각형 거부, 짧은 shadow 버퍼 거부. 실행 결과
  `glide_lfb_region_all=true`(신규 `glide_lfb_region_high_precision_shadow=true`
  포함).
* **런타임 스모크**: `REPIU_GLIDE_LFB_HIGH_PRECISION=1`로 `pumpit8`을 60초 제한
  실행(`REPIU_EXECUTION_TIMEOUT_MS=60000`) — 290프레임 present, 정상 shutdown,
  OSD 초기화 실패 로그 없음, 신규 경로에서 예외 없음. 이 구간은 boot·attract라
  region write 자체는 관측되지 않았으므로, BGA 장면에서의 화질 차이는 실기
  육안 확인이 남아 있다(OSD로 토글 왕복 포함). 이 실행은 기본 off·`F1` 시점의
  것이다.

## 후속 변경 (2026-09-30): OSD 키는 Tab, 고정밀은 기본 on

사용자 지시 두 건을 반영했다.

1. **OSD 키 `F1` → `Tab`.** F1은 JAMMA TEST의 기본 키여서 OSD가 가로채는 동안
   TEST가 게임에 닿지 않았다. `PumpEvents`의 가로채기 조건을 `SDLK_TAB`으로
   바꾸고 OSD 안내 문구를 고쳤다. 결과: F1(과 Ctrl+F1)은 다시 게임으로 가고,
   Tab은 게스트의 BIOS 키보드에 더 이상 닿지 않는다. 가로채기는 키만 보고
   modifier는 보지 않는다.
2. **고정밀 기본 on.** 초기값을 `ResolvePromotedToggle`로 읽고 멤버 초기값을
   `true`로 바꿨다. 끄려면 `REPIU_GLIDE_LFB_HIGH_PRECISION=0`(또는 `off`,
   `false`)이나 OSD 체크박스를 쓴다.

검증:

* Linux x64 Release 빌드 exit 0, core probe `core_probe_all=true`
  (`glide_lfb_region_high_precision_shadow=true`, `glide_lfb_region_all=true`).
* Win32 x86 Debug 빌드 exit 0, core probe `core_probe_all=true`,
  `repiu_aot_probe --glide-lfb-timing` exit 0.
* Linux x64에서 `pumpit8`을 환경 변수 없이(기본값) 60초 실행: 2,364프레임,
  종료 코드 3(시간 제한, 정상), 폴트 없음, `_GRLFBWRITEREGION@32` 480회,
  `_GRLFBREADREGION@28` 480회 — 이번에는 region write가 기본 on 경로를 지났다.

확인하지 않은 것: Tab으로 OSD가 실제로 열리는지와 고정밀 on의 화질은 눈으로 보지
않았다(자동 실행은 키 입력과 화면을 보지 않는다). Win32에서 게임 실행은 하지
않았다.

## 기존 회귀 (이 작업과 무관, 별도 과제)

`repiu_aot_probe`가 `dbt_indirect_dispatch_call_layout=false`,
`dbt_indirect_dispatch_placement=false`로 중단된다. **Task 761 변경을 stash로 걷어낸
깨끗한 트리에서도 동일하게 실패**하므로 main(v0.0.195)에 이미 있던 회귀다.
2026-08-13의 Release probe 바이너리는 통과하므로 그 사이(유력하게는 Task 759의
execution-model 분리)에서 emitter 또는 probe 기대값이 어긋났다. probe 스위트가 이
지점에서 중단되므로 이후 probe들의 회귀 감시가 막혀 있다 — 별도 작업으로 원인을
확정해야 한다. 이번 검증에서는 region probe 호출을 임시로 그 앞에 옮겨 결과를
관측한 뒤 원복했다.

# Task 761 Work Log: Glide LFB High-Precision Path and In-Game OSD Toggle

Implemented as designed: `WriteGlideLfbRegionRgba8` (shared clip/stride rules,
full channels for 888/8888, pack→decode parity for 16-bit sources), the
`ThreadContext` RGBA8 shadow and counters, the boundary's seed/write/flush
wiring, the backend's atomic toggle initialized from
`REPIU_GLIDE_LFB_HIGH_PRECISION` via `ResolvePromotedToggle` (default on; it
was `ResolveOptInToggle` and default off at first, see the follow-up below), and
the new `GlideOsd` subsystem — `Tab` opens and closes it (`F1` at first), Tab
never reaches game input, other input keeps flowing, one checkbox bound to the backend atomic,
rendered just before the buffer swap, absent in dummy mode; `repiu_exe` links
`repiu_imgui` outside the web build.

Verification: Debug builds of `repiu_exe`/`repiu_aot_probe`/`repiu` at exit 0;
`glide_lfb_region_all=true` including the new
`glide_lfb_region_high_precision_shadow=true` (8888 byte preservation, 16-bit
decode parity, clip and short-buffer refusals); a 60-second bounded `pumpit8`
run with the toggle on presented 290 frames and shut down cleanly with no OSD
failure and no faults from the new path. That window is boot/attract, so no
region writes fired; the visual difference on a BGA scene remains a manual
check (including a toggle round trip from the OSD). That run dates from the
default-off, `F1` state.

Follow-up (2026-09-30), two changes the user asked for. The OSD key moved from
`F1` to `Tab`: F1 is JAMMA TEST's default key, so TEST did not reach the game
while the OSD intercepted it. F1 (and Ctrl+F1) go to the game again, Tab no
longer reaches the guest's BIOS keyboard, and the interception looks at the key
only, not at modifiers. High precision is on by default: the initial value is
read with `ResolvePromotedToggle` and the member starts as `true`;
`REPIU_GLIDE_LFB_HIGH_PRECISION=0` (or `off`, `false`) or the OSD checkbox turns
it off. Verification: Linux x64 Release build at exit 0 with
`core_probe_all=true` (including `glide_lfb_region_high_precision_shadow=true`);
Win32 x86 Debug build at exit 0 with `core_probe_all=true` and
`repiu_aot_probe --glide-lfb-timing` at exit 0; a 60-second `pumpit8` run on
Linux x64 with no environment variable presented 2,364 frames, ended with exit
code 3 (the time limit, normal) and no faults, with 480 `_GRLFBWRITEREGION@32`
and 480 `_GRLFBREADREGION@28` calls, so region writes went through the
default-on path this time. Not checked: whether Tab opens the OSD and what the
picture looks like with high precision on were not observed by eye, and no game
run was made on Win32.

Pre-existing regression, unrelated and left for its own task: the probe suite
stops at `dbt_indirect_dispatch_call_layout=false` /
`dbt_indirect_dispatch_placement=false`. A clean tree with Task 761 stashed
fails identically, so the regression predates this task (the 2026-08-13
Release probe binary passes; the Task 759 execution-model split is the likely
window). The suite halting there blinds every later probe; during this task the
region probe was temporarily reordered ahead of it for observation and then
reverted.
