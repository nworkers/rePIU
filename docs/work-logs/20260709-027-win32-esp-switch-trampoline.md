# Win32 ESP 전환 trampoline 작업 로그

## 수행 내용

* `AttemptWin32GuestStackExecution` API를 추가했다.
* Win32 x86 MSVC 전용 naked assembly helper로 guest ESP 전환 후 원본 entry를 호출하도록 구현했다.
* guest stack 위 예외를 안전하게 host로 복구하기 위해 VEH handler와 host recovery stub을 추가했다.
* Win32 loader가 `GuestStackSwitchPlan` 기반 실행 경로를 사용하도록 연결했다.
* 실행 결과 출력에 guest stack switch 지원 여부, 실제 시도 여부, initial ESP, return ESP를 추가했다.
* `docs/TODO.md`에 Win32 x86 guest ESP 전환 trampoline 완료 상태와 다음 잔여 작업을 반영했다.

## 검증

* `cmd /c scripts\build_win32_x86.bat`: 성공
* `build\vs2022_win32_debug\Debug\repiu_loader_win32.exe`: 성공
  * guest stack switch supported: true
  * guest stack switch attempted: true
  * initial ESP: `0x025D6E10`
  * exception code: `0xC0000096`
  * exception address: `0x020F3890`
  * relocated exception byte window 출력 확인
* `build\vs2022_win32_debug\Debug\repiu_exe_analyzer.exe`: 성공
* x64 Visual Studio Debug 빌드: 성공
* `build\vs2022_debug\Debug\repiu_loader_win32.exe`: 성공
  * direct x86 execution unsupported
  * guest stack switch attempted: false

## 회고

guest ESP 전환 자체는 단순한 call trampoline으로 구현할 수 있었지만, 예외가 guest stack 위에서 발생하면 일반 SEH만으로 host stack 복귀가 안정적이지 않았다.
따라서 이번 단계에서 VEH가 exception context의 `EIP`/`ESP`를 host 복구 stub으로 돌려보내는 최소 복구 경로를 추가했다.
다음 단계는 `0x020F3890`의 privileged instruction을 HLE trap으로 처리할지, DOS/4G/DPMI CPU 상태 초기화 요구로 볼지 분류하는 것이다.

# Win32 ESP-Switching Trampoline Work Log

## Work Performed

* Added the `AttemptWin32GuestStackExecution` API.
* Implemented a Win32 x86 MSVC-only naked assembly helper that switches to guest ESP before calling the original entry.
* Added a VEH handler and host recovery stub so exceptions raised on the guest stack can safely return to the host.
* Wired the Win32 loader to use the `GuestStackSwitchPlan` execution path.
* Added execution result output for guest stack switch support, actual attempt state, initial ESP, and return ESP.
* Updated `docs/TODO.md` to record Win32 x86 guest ESP-switching trampoline completion and the next remaining work.

## Verification

* `cmd /c scripts\build_win32_x86.bat`: passed
* `build\vs2022_win32_debug\Debug\repiu_loader_win32.exe`: passed
  * guest stack switch supported: true
  * guest stack switch attempted: true
  * initial ESP: `0x025D6E10`
  * exception code: `0xC0000096`
  * exception address: `0x020F3890`
  * relocated exception byte window printed
* `build\vs2022_win32_debug\Debug\repiu_exe_analyzer.exe`: passed
* x64 Visual Studio Debug build: passed
* `build\vs2022_debug\Debug\repiu_loader_win32.exe`: passed
  * direct x86 execution unsupported
  * guest stack switch attempted: false

## Retrospective

The guest ESP switch itself fit a small call trampoline, but exceptions raised on the guest stack were not reliably recoverable through normal SEH alone.
This step therefore added a minimal VEH path that rewrites exception-context `EIP`/`ESP` to a host recovery stub.
The next step is to classify the privileged instruction at `0x020F3890` as either an HLE trap or a DOS/4G/DPMI CPU-state initialization requirement.
