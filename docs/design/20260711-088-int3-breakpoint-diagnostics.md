# INT3 breakpoint diagnostics 설계

## 배경

`piu_1st`는 FS word memory load 지점을 통과한 뒤 relocated base + `0x000F2098`의 `0xCC`에서 중단된다. Windows 예외 코드는 `0x80000003`이며, 이는 x86 `INT3` breakpoint trap이다.

이 지점은 파일 오픈 실패와 연결된 실패 경로일 가능성이 있지만, 파일 오픈 정책은 별도 작업으로 다룬다. 이번 작업은 `INT3` 자체를 알 수 없는 opcode로 남기지 않고, 원본 코드가 breakpoint trap에 도달했다는 진단으로 명확히 기록하는 것이다.

## 설계

`0xCC`를 실행 가능한 HLE로 넘기지 않는다.

* `PrivilegedInstructionClass`에 guest breakpoint trap 분류를 추가한다.
* `ClassifyPrivilegedInstruction`이 `0xCC`를 `INT3`로 분류한다.
* loader 출력은 `INT3`를 `unknown`이 아니라 `breakpoint`로 표시한다.
* 현재 실행 블로커 메시지는 `guest breakpoint trap`으로 고정한다.
* 예외 발생 시 `EAX..EDI`뿐 아니라 `EIP`, `ESP`, `EBP`, `EFLAGS`, segment register까지 스냅샷으로 출력한다.

`INT3`에서 `EIP += 1`로 계속 진행하는 옵션은 이번 범위에서 추가하지 않는다. 기본 정책은 원본 코드가 실패/중단 지점에 도달했음을 보고하고 실행을 종료하는 것이다.

## 기대 결과

`piu_1st` 실행 결과는 여전히 `0x80000003`에서 멈추지만, 로그는 다음 내용을 명확히 표현한다.

* opcode `0xCC`
* mnemonic `INT3`
* instruction class `guest breakpoint trap`
* current execution blocker `guest breakpoint trap`
* 확장된 예외 레지스터/segment 스냅샷

# INT3 Breakpoint Diagnostics Design

## Background

After passing the FS word memory load point, `piu_1st` stops at `0xCC` at relocated base + `0x000F2098`. The Windows exception code is `0x80000003`, which is the x86 `INT3` breakpoint trap.

This point is likely connected to the file-open failure path, but file-open policy is handled separately. This task makes the `INT3` itself a clear diagnostic event instead of leaving it as an unknown opcode.

## Design

Do not continue execution past `0xCC` as HLE.

* Add a guest breakpoint trap classification to `PrivilegedInstructionClass`.
* Let `ClassifyPrivilegedInstruction` classify `0xCC` as `INT3`.
* Print `INT3` as `breakpoint`, not `unknown`.
* Use `guest breakpoint trap` as the current execution blocker message.
* Print an exception snapshot including `EIP`, `ESP`, `EBP`, `EFLAGS`, and segment registers in addition to `EAX..EDI`.

This task does not add an `EIP += 1` continue option. The default policy is to report that the original code reached a failure/trap point and then stop execution.

## Expected Result

`piu_1st` still stops at `0x80000003`, but the log clearly reports:

* opcode `0xCC`
* mnemonic `INT3`
* instruction class `guest breakpoint trap`
* current execution blocker `guest breakpoint trap`
* expanded exception register/segment snapshot
