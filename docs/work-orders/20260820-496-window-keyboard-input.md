# 20260820-496 창 키보드 BIOS 입력 작업 지시서 / Window keyboard BIOS input work order

## 한국어

### 목적

게임 창이 받은 키보드 입력을 DOS 환경의 BIOS 키보드처럼 원본 게임의 `INT 16h` 경로로
전달합니다.

### 작업

- [x] 기존 SDL/JAMMA event pump와 `INT 16h` 호출 경로를 조사합니다.
- [x] BIOS FIFO, modifier 상태, SDL 변환 경계를 설계합니다.
- [x] 공용 BIOS 키보드 상태와 SDL adapter를 구현합니다.
- [x] `INT 16h AH=00/01/02/10/11/12`를 실제 상태에 연결합니다.
- [x] 회귀 probe를 추가하고 Win32 빌드 및 관련 probe를 실행합니다.
- [x] 아키텍처, 분석, 지식 기반 및 작업 로그를 갱신합니다.
- [ ] 변경을 작업 브랜치에 커밋합니다. (도구 승인 한도로 보류)

### 범위

원본 실행 파일, 게임 로직, JAMMA 입력 의미는 변경하지 않습니다. IBM PC BIOS에서 표현되는
키 입력과 shift/lock 상태만 HLE로 제공합니다. raw keyboard controller `INT 09h` 실행은 이번
범위에 포함하지 않습니다. 확인된 게임 입력 경로가 `INT 16h`를 직접 폴링하기 때문입니다.

### 최소 검증

전용 BIOS keyboard probe, 전체 AOT probe, Win32 x86 Debug 빌드를 통과해야 합니다. runtime
smoke가 불가능하면 이유를 작업 로그에 기록합니다.

## English

### Objective

Deliver keyboard input received by the game window to the original game's `INT 16h` path as a DOS
BIOS keyboard would.

### Work items

- [x] Inspect the existing SDL/JAMMA event pump and `INT 16h` call path.
- [x] Design the BIOS FIFO, modifier state, and SDL translation boundary.
- [x] Implement shared BIOS keyboard state and the SDL adapter.
- [x] Connect `INT 16h AH=00/01/02/10/11/12` to real state.
- [x] Add regression probes and run the Win32 build and relevant probe suite.
- [x] Update architecture, analysis, knowledge-base, and work-log documents.
- [ ] Commit the change on the task branch. (pending due to tool approval limit)

### Scope

Do not modify the original executable, game logic, or JAMMA semantics. HLE only keyboard input and
shift/lock state representable by an IBM PC BIOS. Raw keyboard-controller `INT 09h` execution is
out of scope because the confirmed game input path polls `INT 16h` directly.

### Minimum verification

Pass the dedicated BIOS keyboard probe, full AOT probe, and Win32 x86 Debug build. Record why if a
runtime smoke test cannot be performed.
