# STI HLE trap 계속 실행 작업 로그

## 결과

Win32 execution trampoline에서 privileged trap HLE와 DOS HLE를 분리했다.
`piu_1st` 실행은 이제 DOS console HLE 전체를 켜지 않고도 `CLI`/`STI` 같은 privileged trap 후보만 처리할 수 있다.

`STI` 처리에서는 Win32 `CONTEXT.EFlags`의 interrupt flag bit를 세우고 `Eip`를 1 증가시킨다.
`CLI` 처리에서는 interrupt flag bit를 끄고 `Eip`를 1 증가시킨다.
execution attempt에는 처리된 HLE trap count와 마지막 처리 EIP/opcode를 기록한다.

loader는 `piu_1st`에 `AttemptWin32GuestStackTrapExecution` 경로를 사용한다.
`dos4gw_hello`는 기존 DOS HLE 경로를 유지하며, 내부적으로 privileged trap HLE도 함께 사용한다.

## 검증

* `cmd /c scripts\build_win32_x86.bat`: 성공
  * `spdlog` 외부 헤더의 MSVC C4819 경고는 유지되지만 빌드는 성공했다.
* `build\vs2022_win32_debug\Debug\repiu_loader_win32.exe piu_1st`: 성공
  * handled HLE trap count: `1`
  * last handled HLE trap address: `0x020F3890`
  * last handled HLE trap opcode: `0xFB`
  * 기존 `STI` 지점을 통과한 뒤 다음 중단 지점 `0x020F38B5`에 도달했다.
  * 다음 중단 지점의 opcode는 `0xCD`, `INT imm8`이며 byte window상 `CD 21`이다.
  * exception code는 `0xC0000005`로 기록되었다.
* `build\vs2022_win32_debug\Debug\repiu_loader_win32.exe dos4gw_hello`: 성공
  * stdout의 `Hello, world!` 출력 유지
  * handled HLE trap count: `1`

## 판단

첫 privileged trap인 `STI`는 HLE 처리로 넘길 수 있음이 확인되었다.
이제 `piu_1st`의 다음 실제 요구사항은 `INT 21h`이다.
현재 byte window 직전 명령이 `B4 30`이므로 첫 관찰 함수는 DOS version query인 `AH=0x30`으로 볼 수 있다.

다음 작업은 `INT 21h AH=0x30`을 시작점으로 DOS HLE dispatcher 호출/복귀 경로를 `piu_1st`에도 연결하는 것이다.

# STI HLE Trap Continuation Work Log

## Result

Separated privileged trap HLE from DOS HLE in the Win32 execution trampoline.
`piu_1st` can now handle privileged trap candidates such as `CLI`/`STI` without enabling the entire DOS console HLE path.

`STI` handling sets the interrupt flag bit in Win32 `CONTEXT.EFlags` and advances `Eip` by 1.
`CLI` handling clears the interrupt flag bit and advances `Eip` by 1.
The execution attempt records handled HLE trap count and the last handled EIP/opcode.

The loader uses `AttemptWin32GuestStackTrapExecution` for `piu_1st`.
`dos4gw_hello` keeps the existing DOS HLE path, which also enables privileged trap HLE internally.

## Verification

* `cmd /c scripts\build_win32_x86.bat`: passed
  * MSVC C4819 warnings from external `spdlog` headers remain, but the build passed.
* `build\vs2022_win32_debug\Debug\repiu_loader_win32.exe piu_1st`: passed
  * handled HLE trap count: `1`
  * last handled HLE trap address: `0x020F3890`
  * last handled HLE trap opcode: `0xFB`
  * execution continued past the previous `STI` stop and reached the next stop at `0x020F38B5`
  * the next opcode is `0xCD`, `INT imm8`, and the byte window shows `CD 21`
  * exception code recorded as `0xC0000005`
* `build\vs2022_win32_debug\Debug\repiu_loader_win32.exe dos4gw_hello`: passed
  * Preserved `Hello, world!` on stdout
  * handled HLE trap count: `1`

## Judgment

The first privileged trap, `STI`, is confirmed to be safe to pass through HLE handling.
The next real requirement exposed by `piu_1st` is now `INT 21h`.
The preceding byte pattern contains `B4 30`, so the first observed DOS function is likely DOS version query `AH=0x30`.

The next task is to connect DOS HLE dispatcher call/return handling for `INT 21h AH=0x30` on the `piu_1st` path.
