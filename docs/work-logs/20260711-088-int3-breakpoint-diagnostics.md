# INT3 breakpoint diagnostics 작업 로그

## 결과

`piu_1st`가 `INT3` opcode `0xCC`에서 중단될 때 이를 unknown opcode가 아니라 guest breakpoint trap으로 분류하도록 변경했다.

* `PrivilegedInstructionClass::kGuestBreakpointTrap`을 추가했다.
* privileged instruction classifier가 `0xCC`를 `INT3`로 분류하도록 했다.
* loader classification 출력에서 breakpoint trap을 error 레벨로 표시하고, 현재 블로커를 `guest breakpoint trap`으로 기록한다.
* 예외 시점의 `X86ExecutionSnapshot`을 `Win32MinimalExecutionAttempt`에 복사한다.
* 예외 로그에 `EIP`, `ESP`, `EBP`, `EFLAGS`, `CS`, `DS`, `ES`, `SS`, `FS`, `GS`를 추가로 출력한다.
* `scripts/test_all.ps1`의 `piu_1st` 기대값을 새 breakpoint 진단 로그에 맞춰 갱신했다.

## 정책

이번 변경은 `INT3`를 처리해서 계속 실행하지 않는다. `INT3`는 원본 코드가 중단/실패 지점에 도달했다는 진단 신호로 보고, 최대한 많은 상태를 출력한 뒤 현재처럼 실행을 종료한다.

파일 오픈 실패 원인 수정과 DOS open trace buffer 확장은 이번 범위에서 제외했다.

## 검증

다음 명령으로 검증했다.

* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1 -SkipSetup`

결과는 통과했다. Win32 x86 빌드 중 third-party `spdlog` header의 기존 코드 페이지 경고 `C4819`가 계속 표시되지만 이번 변경과 직접 관련된 실패는 아니다.

## 관찰된 현재 블로커

* exception code: `0x80000003`
* exception address: relocated base + `0x000F2098`
* opcode: `0xCC`
* mnemonic: `INT3`
* instruction class: `guest breakpoint trap`
* current execution blocker: `guest breakpoint trap`

# INT3 Breakpoint Diagnostics Work Log

## Result

Changed the `piu_1st` `INT3` opcode `0xCC` stop so it is classified as a guest breakpoint trap instead of an unknown opcode.

* Added `PrivilegedInstructionClass::kGuestBreakpointTrap`.
* Classified `0xCC` as `INT3` in the privileged instruction classifier.
* Printed breakpoint traps at error level and recorded the current blocker as `guest breakpoint trap`.
* Copied the exception-time `X86ExecutionSnapshot` into `Win32MinimalExecutionAttempt`.
* Added `EIP`, `ESP`, `EBP`, `EFLAGS`, `CS`, `DS`, `ES`, `SS`, `FS`, and `GS` to the exception log.
* Updated the `piu_1st` expectations in `scripts/test_all.ps1` for the new breakpoint diagnostics.

## Policy

This change does not continue execution after `INT3`. `INT3` is treated as a diagnostic signal that the original code reached a stop/failure point. The host dumps as much state as practical and then ends the current execution attempt.

Fixing the file-open failure and expanding the DOS open trace buffer are out of scope for this task.

## Verification

Verified with:

* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1 -SkipSetup`

The verification passed. The Win32 x86 build still reports the existing third-party `spdlog` code-page warning `C4819`, but it is not a failure caused by this change.

## Observed Current Blocker

* exception code: `0x80000003`
* exception address: relocated base + `0x000F2098`
* opcode: `0xCC`
* mnemonic: `INT3`
* instruction class: `guest breakpoint trap`
* current execution blocker: `guest breakpoint trap`
