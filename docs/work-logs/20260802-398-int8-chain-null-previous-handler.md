# 20260802-398 INT 8 체인 인식 조건 수정 작업 로그 / INT 8 Chain Recognition Fix Work Log

## 한국어

### 작업 요약

pumpit3가 자신의 INT 8 ISR 안에서 이전 핸들러로 체인하는 `pushf` + `call far`에서
종료하던 문제를 사용자 제공 로그와 원본 실행 파일 대조로 확정하고,
`HandleTimerInterruptChainBoundary`의 인식 조건을 selector 규칙으로 교체했습니다.

### Task 397 수정의 실행 결과

- `INT 21h AH=2Ah/2Ch`가 모두 처리되어 `0x030D3941`과 `0x030D2CA8`을 통과했습니다.
- 파일 5개 열기, 읽기 12회, `STAGE.CFG` seek/read, 메모리 resize 59회,
  DOS IOCTL 16회를 수행했습니다.
- Glide 게이트 51회 진입, `_GRSSTWINOPEN@28: opened=1 message=640x480` 창 생성까지
  도달했습니다.
- PIT은 divisor 23(51.9kHz) → 4971(240Hz) 순으로 프로그램됐습니다.

### 원인 확정 근거

1. 정지 지점 `0x0301F827`, `0xC0000005`. byte window는 파일 offset `0x2AA27`과 일치합니다.
2. 이 함수는 `out 0x20,al`(PIC EOI) / `sti` / `pop gs,fs,es,ds` / `popad` / `iretd`로
   끝나는 IRQ 핸들러이며, 로그의
   `DOS INT 21h AH=25h vector 0x08 set to 0023:0301F7BC`가 INT 8 ISR임을 확정합니다.
3. 문제 명령은 `pushf` + `call far [0x0343ED08]` — 이전 핸들러 체인 관용구입니다.
4. `Win32 INT 8 chain HLE count`는 0이었습니다. `HandleTimerInterruptChainBoundary`가
   `target_offset == 0 && target_selector != 0 && target_selector == DS`를 요구하는데
   pumpit3가 저장한 값은 `0000:03010000`이기 때문입니다.
5. 저장 값의 출처: 게스트 wrapper `0x030D0963`이 `int 21h`(AH=35h) 후 `mov eax, ebx`로
   EBX 전체를 offset으로 사용하는데, `HandleDosGetInterruptVector`는 하위 16비트만
   기록합니다. 진입 시 `EBX = 0x0301F7BC`였으므로 상위 절반이 남아 `0x03010000`이
   반환됐습니다.

### 변경 내용

1. `src/platform/win32/boundary/timer_interrupt_boundary.cpp`:
   `HandleTimerInterruptChainBoundary`의 인식 조건을 `target_selector != CS`로 교체.
   게스트 코드 selector는 `CS`(`0x0023`)뿐이므로 `0`, `DS`, `FS` 어느 것도 far call
   대상이 될 수 없습니다. pumpit1의 `002B:00000000`과 pumpit3의 `0000:03010000`이 한
   규칙으로 덮이고 타이틀별 offset에 의존하지 않습니다. selector가 `CS`인 진짜 체인은
   계속 fail-closed입니다. 동작(EFLAGS 한 개 제거 후 far call 다음으로 진행)은 그대로입니다.
2. 문서: 설계, 작업 지시, `interrupts-and-port-io.md`,
   `current-execution-frontier.md` 갱신.

### 검증 결과

- 빌드: `cmake --build build --config Release --target repiu_loader_win32` 성공,
  신규 경고 없음.
- **실행 검증 대기 중.** pumpit3 로그에서 `Win32 INT 8 chain HLE count` 증가와
  `0x0301F827` 통과, pumpit1/pumpit2의 `INT 8 chain HLE count` 회귀 없음을 확인해야
  합니다.

### 별도 Task로 남긴 것

`HandleDosGetInterruptVector`의 `EBX` 하위 16비트 절단은 확인된 별개 결함입니다.
`HandleDosSetInterruptVector`가 `dpmi_entry.offset = win32_context->Edx`로 32비트 전체를
저장하는 것과 비대칭이며, 32비트 DPMI 클라이언트에게 상위 절반 쓰레기 값을 전달합니다.
pumpit1/pumpit2와 공유하는 경로를 바꾸므로 세 타이틀 회귀 검증을 포함한 별도 Task로
분리했습니다. 본 Task의 selector 규칙은 그 수정 이후에도 성립합니다.

---

## English

### Summary

Confirmed, from the user-provided log cross-checked against the original executable, why
pumpit3 terminated on the `pushf` + `call far` that chains to the previous handler inside
its own INT 8 ISR, and replaced the `HandleTimerInterruptChainBoundary` recognition
condition with a selector rule.

### What the Task 397 fix produced at runtime

- Both `INT 21h AH=2Ah` and `AH=2Ch` were serviced, clearing `0x030D3941` and `0x030D2CA8`.
- Five file opens, twelve reads, `STAGE.CFG` seek/read, 59 memory resizes, 16 DOS IOCTLs.
- 51 Glide gate entries and `_GRSSTWINOPEN@28: opened=1 message=640x480`.
- PIT programmed divisor 23 (51.9 kHz) then 4971 (240 Hz).

### Evidence for the root cause

1. The stop is `0x0301F827` with `0xC0000005`; the byte window matches file offset
   `0x2AA27`.
2. The function ends with `out 0x20,al` (PIC EOI), `sti`, `pop gs,fs,es,ds`, `popad`,
   `iretd`, making it an IRQ handler, and
   `DOS INT 21h AH=25h vector 0x08 set to 0023:0301F7BC` identifies it as the INT 8 ISR.
3. The faulting instruction is `pushf` + `call far [0x0343ED08]`, the chain idiom.
4. `Win32 INT 8 chain HLE count` was zero because
   `HandleTimerInterruptChainBoundary` required
   `target_offset == 0 && target_selector != 0 && target_selector == DS`, while pumpit3
   saved `0000:03010000`.
5. Origin of that value: the guest wrapper at `0x030D0963` runs `int 21h` (AH=35h) then
   `mov eax, ebx`, using the full EBX as the previous offset, but
   `HandleDosGetInterruptVector` writes only the low 16 bits. EBX held `0x0301F7BC` on
   entry, so the stale high half produced `0x03010000`.

### Changes

1. `src/platform/win32/boundary/timer_interrupt_boundary.cpp`: the recognition condition is
   now `target_selector != CS`. `CS` (`0x0023`) is the only guest code selector, so `0`,
   `DS`, and `FS` cannot be far-call targets. One rule covers pumpit1's `002B:00000000` and
   pumpit3's `0000:03010000` without per-title offsets, and a genuine `CS` chain stays
   fail-closed. The action — discard one EFLAGS and resume after the far call — is unchanged.
2. Documentation: design, work order, `interrupts-and-port-io.md`, and
   `current-execution-frontier.md`.

### Verification results

- Build: `cmake --build build --config Release --target repiu_loader_win32` succeeded with
  no new warnings.
- **Runtime verification pending.** A pumpit3 log should show `Win32 INT 8 chain HLE count`
  rising and execution past `0x0301F827`, with no regression to the pumpit1/pumpit2 counts.

### Left to a separate task

The low-16-bit `EBX` truncation in `HandleDosGetInterruptVector` is a confirmed separate
defect, asymmetric with `HandleDosSetInterruptVector` storing the full 32 bits, and it hands
a 32-bit DPMI client a stale high half. It changes a path shared with pumpit1 and pumpit2, so
it was split into its own task with three-title regression verification. The selector rule in
this task still holds after that fix.
