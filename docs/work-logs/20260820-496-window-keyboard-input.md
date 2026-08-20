# 20260820-496 창 키보드 BIOS 입력 작업 로그 / Window keyboard BIOS input work log

## 한국어

### 결과

게임 창의 SDL 키보드 event를 원본 게임이 프레임마다 조회하는 `INT 16h` BIOS 경로에
연결했습니다. 기존 JAMMA 입력은 그대로 병행 동작합니다.

### 변경

- `hle::BiosKeyboard`: thread-safe 15-entry FIFO, legacy/enhanced AX, shift flags와
  accepted/consumed/overflow snapshot을 추가했습니다.
- `sdl_bios_keyboard_adapter`: US 101-key scan/ASCII, modifier 조합, function/navigation/keypad,
  typematic repeat와 focus-loss 해제를 구현했습니다.
- SDL event pump가 모든 key down/up을 BIOS adapter에 전달하고 실행 컨텍스트의 키보드
  상태를 backend와 공유하도록 연결했습니다.
- `INT 16h AH=00/01/10/11`이 FIFO를 read/peek하고 `AH=02/12`가 실제 shift flags를
  반환하도록 Task 401의 빈 상태 구현을 확장했습니다.
- 전용 `--bios-keyboard-input` probe를 추가했습니다.

### 검증

- Win32 x86 Debug `repiu_exe`, `repiu_aot_probe`, `repiu`: 빌드 성공.
- `repiu_aot_probe.exe --bios-keyboard-input`: 성공.
  `translated=true,fifo=true,modifiers=true,overflow=1`.
- `repiu_aot_probe.exe --jamma-input-timeline`: 성공. PIT timer/delivery와 JAMMA timeline 전체
  항목이 true입니다.
- `repiu_aot_probe.exe build/runtime_mounts/pumpit1/PIU/PIU.EXE`: 전체 probe 성공.
- `git diff --check`: 오류 없음. 줄 끝 변환 경고만 있습니다.
- `pumpit3` runtime smoke: 로컬에 `pumpit3` CHD directory가 없어 실행 준비 단계에서
  중단됐습니다. 창에서 실제 키를 누르는 최종 확인은 자산이 있는 환경에서 필요합니다.

초기 빌드 제한 시간이 자식 MSBuild를 남겨 빌드가 충돌했습니다. 이 작업에서 시작된 동일
시각의 orphan process만 종료하고 `/m:1`로 재실행하여 정상 완료했습니다.

Git 스테이징은 사용자 소유 `repiu.exe`, `repiu150.exe`를 제외한 명시적 파일 목록으로
요청했지만 도구 승인 사용 한도로 거절되었습니다. 변경은 작업 브랜치에 보존했으며 커밋은
수행하지 못했습니다.

## English

### Result

Connected SDL keyboard events from the game window to the `INT 16h` BIOS path polled by the
original game every frame. Existing JAMMA input continues in parallel.

### Changes

- Added `hle::BiosKeyboard`: a thread-safe 15-entry FIFO, legacy/enhanced AX values, shift flags,
  and accepted/consumed/overflow snapshots.
- Added `sdl_bios_keyboard_adapter` with US 101-key scan/ASCII translation, modifier combinations,
  function/navigation/keypad handling, typematic repeats, and focus-loss release.
- Routed every SDL key down/up through the adapter and shared execution-context keyboard state with
  the window backend.
- Extended Task 401 so `INT 16h AH=00/01/10/11` reads/peeks the FIFO and `AH=02/12` returns real
  shift flags.
- Added the dedicated `--bios-keyboard-input` probe.

### Verification

- Win32 x86 Debug `repiu_exe`, `repiu_aot_probe`, and `repiu`: build passed.
- `repiu_aot_probe.exe --bios-keyboard-input`: passed with
  `translated=true,fifo=true,modifiers=true,overflow=1`.
- `repiu_aot_probe.exe --jamma-input-timeline`: passed, including every PIT timer/delivery and
  JAMMA timeline check.
- `repiu_aot_probe.exe build/runtime_mounts/pumpit1/PIU/PIU.EXE`: full probe passed.
- `git diff --check`: no errors; only line-ending conversion warnings.
- `pumpit3` runtime smoke stopped during setup because the local pumpit3 CHD directory is absent.
  Final live key confirmation requires an environment with the assets.

The first timed build left child MSBuild processes that conflicted with later builds. Only orphan
processes started by this task at the same timestamp were terminated; rebuilding with `/m:1` then
completed normally.

Git staging explicitly excluded the user-owned `repiu.exe` and `repiu150.exe`, but the request was
rejected by the tool approval usage limit. Changes remain preserved on the task branch and could
not be committed.
