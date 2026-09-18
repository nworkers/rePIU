# Task 711 작업 지시 — PIU.BIN 읽기 루프의 근인 추적

설계: [20260918-711](../design/20260918-711-piu-bin-loop-ss-override.md)

## 범위

진단 작업이다. 코드 변경은 DOS file I/O trace의 스택 캡처 폭 하나다. 루프의 근인을
찾으면 그 수정은 **이 작업에서 하지 않고** 설계를 따로 세운다.

## 단계

1. `include/repiu/engine/execution_trampoline.h`의
   `DosFileIoTraceEntry::guest_stack`을 `[8]`에서 `[24]`로 넓힌다.
2. `PIU.EXE` object 2를 추출해 read 호출자 체인을 역어셈블한다.
3. 넓힌 캡처로 게임 쪽 루프를 특정하고 탈출 조건을 읽는다.
4. 가설을 커밋하지 않는 실험으로 판정한다.
5. 작업 로그, `docs/analysis/linux-port-frontier.md`, 필요하면
   `docs/EXE_DESIGN.ko.md` / `docs/EXE_DESIGN.en.md`.

## 검증

* Linux x64, Win32 x86 빌드와 core probe 전체

## 완료 조건

* 루프의 탈출 조건과, Linux에서 그것이 성립하지 않는 이유가 실험으로 확인된다.

---

## English

Design: [20260918-711](../design/20260918-711-piu-bin-loop-ss-override.md)

### Scope

A diagnostic task. The only code change is the width of the DOS file-I/O trace's
stack capture. If the loop's cause is found, its fix is **not made here**; it gets
its own design.

### Steps

1. Widen `DosFileIoTraceEntry::guest_stack` in
   `include/repiu/engine/execution_trampoline.h` from `[8]` to `[24]`.
2. Extract `PIU.EXE` object 2 and disassemble the read's caller chain.
3. Use the widened capture to single out the game's loop and read its exit
   condition.
4. Settle the hypothesis with an uncommitted experiment.
5. Work log, `docs/analysis/linux-port-frontier.md`, and
   `docs/EXE_DESIGN.ko.md` / `docs/EXE_DESIGN.en.md` where relevant.

### Verification

* Linux x64 and Win32 x86 builds and every core-probe group.

### Done when

* The loop's exit condition, and why it does not hold on Linux, are confirmed by
  experiment.
