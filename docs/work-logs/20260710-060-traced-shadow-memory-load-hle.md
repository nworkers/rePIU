# Traced shadow memory load HLE 작업 로그

## 변경 내용

out-of-arena store를 byte-addressed shadow memory에 기록하도록 했다. `C7`, `66 C7`, `D9 FST/FSTP` skipped store는 shadow memory에 반영되며, `8B /r` dword memory load는 runtime arena 또는 shadow memory에서 값을 읽어 register destination에 기록한다.

minimal execution timeout 시 `TerminateThread` 결과와 후속 wait를 확인하도록 보강했다. timeout 상태에서 guest thread가 종료되지 않으면 메시지에 실패 원인을 남긴다.

`scripts/test_all.ps1`의 capture 실행은 PowerShell 파이프 직접 실행 대신 `System.Diagnostics.Process` 기반 실행으로 바꿨다. stdout/stderr를 비동기로 수집하고, 지정된 timeout을 넘기면 child process를 종료하도록 해 테스트 스크립트 자체가 무기한 대기하지 않게 한다.

## 결과

`0x0201DF24`의 `8B 50 18` load 이후 더 이상 새 opcode 예외가 잡히지 않고, `piu_1st`가 minimal execution timeout에 도달한다. 현재는 다음 opcode를 특정할 수 없으므로 opcode 추가 방식으로는 여기서 진행을 멈춘다.

## 검증

* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
  * 결과: `Win32 minimal execution timed out: true`
  * `Win32 minimal execution message: minimal execution attempt timed out`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`
  * 이후 검증은 남은 `repiu_loader_win32.exe` timeout 프로세스들이 실행 파일을 잠가 `LNK1168`로 실패했다.
  * `Stop-Process`와 `taskkill` 모두 접근 거부로 남은 프로세스를 종료하지 못했다.

## 다음 작업

timeout 상태에서는 예외 byte window가 남지 않는다. 다음 단계는 opcode 추가가 아니라 timeout된 guest thread를 안전하게 중단하고 마지막 guest `EIP` 또는 주기적 실행 trace를 남기는 관측 장치를 설계하는 것이다.

# Traced Shadow Memory Load HLE Work Log

## Changes

Out-of-arena stores are now recorded in byte-addressed shadow memory. Skipped stores from `C7`, `66 C7`, and `D9 FST/FSTP` are reflected into shadow memory, and `8B /r` dword memory loads read from either the runtime arena or shadow memory before writing the register destination.

Minimal execution timeout handling now checks the `TerminateThread` result and the following wait. If the guest thread does not terminate after timeout, the failure reason is recorded in the message.

`scripts/test_all.ps1` capture execution now uses `System.Diagnostics.Process` instead of direct PowerShell pipeline execution. It collects stdout/stderr asynchronously and kills the child process when the timeout is exceeded, so the test script itself does not wait indefinitely.

## Result

After the `8B 50 18` load at `0x0201DF24`, no new opcode exception is caught. Instead, `piu_1st` reaches the minimal execution timeout. There is no next opcode to identify, so opcode-addition work stops here.

## Verification

* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
  * Result: `Win32 minimal execution timed out: true`
  * `Win32 minimal execution message: minimal execution attempt timed out`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`
  * Later verification failed with `LNK1168` because remaining timed-out `repiu_loader_win32.exe` processes locked the executable.
  * Both `Stop-Process` and `taskkill` failed to terminate the remaining processes due to access denial.

## Next Work

The timeout state does not leave an exception byte window. The next step is not adding another opcode, but designing an observation tool that safely stops the timed-out guest thread and records the last guest `EIP` or periodic execution trace.
