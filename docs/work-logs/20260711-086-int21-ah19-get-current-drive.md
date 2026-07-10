# INT 21h AH=19h 현재 드라이브 조회 작업 로그

## 결과

DOS `INT 21h AH=19h` 현재 drive 조회를 HLE에 추가했다. 현재 단일 DOS virtual filesystem root를 `C:`로 노출하므로 `AL=0x02`를 반환한다.

일반 `INT 21h` 처리 경로와 traced `INT 21h` 처리 경로 모두 같은 helper를 사용한다. 또한 get drive 전용 관측 로그를 추가했다.

## 관측

`piu_1st`는 기존 중단점인 `INT 21h AH=19h`를 통과했다.

로그에는 다음 값이 표시된다.

* `Win32 handled DOS get drive count: 1`
* `Win32 last DOS get drive value: 0x02`

다음 중단점은 relocated base + `0x000F246F`이며, 예외 바이트는 `66 65 8B 10 ...`이다. 이는 DOS interrupt가 아니라 FS segment override memory load 계열로 보인다.

## 검증

* 통과: `cmd /c scripts\build_win32_x86.bat`
* 통과: `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
* 통과: `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1 -SkipSetup`
* 참고: Win32 x86 빌드 중 서드파티 `spdlog` 헤더의 기존 `C4819` 경고가 출력되었다.

# INT 21h AH=19h Get Current Drive Work Log

## Result

DOS `INT 21h AH=19h` get current drive was added to the HLE. The current single DOS virtual filesystem root is exposed as `C:`, so the handler returns `AL=0x02`.

Both the normal `INT 21h` path and the traced `INT 21h` path use the same helper. Dedicated get-drive observation logging was also added.

## Observation

`piu_1st` passed the previous `INT 21h AH=19h` blocker.

The log now shows:

* `Win32 handled DOS get drive count: 1`
* `Win32 last DOS get drive value: 0x02`

The next blocker is at relocated base + `0x000F246F`, with exception bytes `66 65 8B 10 ...`. This appears to be an FS segment override memory load rather than a DOS interrupt.

## Verification

* Passed: `cmd /c scripts\build_win32_x86.bat`
* Passed: `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
* Passed: `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1 -SkipSetup`
* Note: The Win32 x86 build still emits the existing third-party `spdlog` header `C4819` warning.
