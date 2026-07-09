# loader 로그 레벨 분류 작업 로그

## 결과

`piu_1st` 실행 로그에서 사용자가 현재 막힌 지점을 쉽게 구분할 수 있도록 Win32 loader 로그 레벨을 조정했다.

구현 내용은 다음과 같다.

* fixed low address range probe 실패를 `warn`으로 출력하게 했다.
* fixed range reservation 실패와 Windows error를 `warn`으로 출력하게 했다.
* original entry 실행 중 잡힌 SEH 예외 정보를 `error`로 출력하게 했다.
* 예외 위치의 relocated byte window를 `error`로 출력하게 했다.
* 예외 위치 instruction classification이 unknown이면 current blocker 메시지를 `error`로 출력하게 했다.
* 정상 반환 경로와 일반 진행 상태는 `info`로 유지했다.
* `ARCHITECTURE.md`에 Win32 loader log level policy를 추가했다.

## 검증

`powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`

결과: 성공.

* Win32 x86 Debug 빌드 성공.
* `dos4gw_hello` target 실행 성공.
  * 정상 반환 경로이며 current blocker `error`는 출력되지 않았다.
  * fixed address range 우회 항목은 `warning`으로 출력된다.
* `piu_1st` target 실행 성공.
  * fixed address range 우회 항목은 `warning`으로 출력된다.
  * 현재 실행 중단점은 `error`로 출력된다.
  * exception address: `0x020F4D7D`
  * exception byte window focus: `26 8A 4F FF`
  * current blocker: `unhandled or unclassified instruction/memory access at exception point`

기존과 동일하게 외부 `spdlog` header에서 MSVC C4819 경고가 발생할 수 있으나 빌드는 성공한다.

## 다음 작업

다음 구현 작업은 `26 8A 4F FF` segment-override byte memory load를 selector shadow state와 주소 변환 정책으로 처리하는 것이다.

# Loader Log Level Classification Work Log

## Result

Adjusted Win32 loader log levels so users can easily distinguish the current blocker in `piu_1st` execution logs.

Implemented changes:

* Fixed low address range probe failure is now printed as `warn`.
* Fixed range reservation failure and Windows error are now printed as `warn`.
* SEH exception details caught during original entry execution are now printed as `error`.
* The relocated byte window at the exception point is now printed as `error`.
* When instruction classification at the exception point is unknown, a current blocker message is printed as `error`.
* Normal return paths and ordinary progress remain `info`.
* Added the Win32 loader log level policy to `ARCHITECTURE.md`.

## Verification

`powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`

Result: success.

* Win32 x86 Debug build succeeded.
* `dos4gw_hello` target run succeeded.
  * This is a normal return path and does not print a current blocker `error`.
  * Fixed address range fallback items are printed as `warning`.
* `piu_1st` target run succeeded.
  * Fixed address range fallback items are printed as `warning`.
  * The current execution stop is printed as `error`.
  * exception address: `0x020F4D7D`
  * exception byte window focus: `26 8A 4F FF`
  * current blocker: `unhandled or unclassified instruction/memory access at exception point`

As before, the external `spdlog` header may emit MSVC C4819 warnings, but the build succeeds.

## Next Work

The next implementation task is handling the `26 8A 4F FF` segment-override byte memory load through selector shadow state and address translation policy.
