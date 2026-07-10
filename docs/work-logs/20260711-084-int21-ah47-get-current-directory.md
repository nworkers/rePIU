# INT 21h AH=47h 현재 디렉터리 조회 작업 로그

## 결과

DOS virtual filesystem에 현재 디렉터리 문자열 조회 helper를 추가했다. `DosVirtualFileSystemState::current_components`를 DOS `AH=47h` 반환 형식에 맞춰 drive와 선행 `\` 없는 문자열로 변환한다.

Win32 execution trampoline은 `INT 21h AH=47h`에서 `DS:SI` 버퍼에 ASCIZ 문자열을 쓰고 carry flag를 clear한다. 현재 `piu_1st` 실행 흐름에서는 `\DATAS\BGA` 상태가 `DATAS\BGA`로 반환된다.

## 관측

`piu_1st`는 기존 중단점인 `INT 21h AH=47h`를 통과했다. 다음 중단점은 relocated base + `0x000F423B`의 `CD 21`이며, `EAX=0x00001900`이다.

이는 `INT 21h AH=19h`, 현재 drive 조회 요청이다. 현재 디렉터리 문자열 조회 이후 원본 코드가 drive 문자와 경로 문자열을 조립하려는 흐름으로 보인다.

## 검증

* 통과: `cmd /c scripts\build_win32_x86.bat`
* 통과: `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
* 통과: `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1 -SkipSetup`
* 참고: Win32 x86 빌드 중 서드파티 `spdlog` 헤더의 기존 `C4819` 경고가 출력되었다.

# INT 21h AH=47h Get Current Directory Work Log

## Result

A current-directory string helper was added to the DOS virtual filesystem. It converts `DosVirtualFileSystemState::current_components` into the DOS `AH=47h` return format, excluding the drive and leading `\`.

The Win32 execution trampoline now writes the ASCIZ string to the `DS:SI` buffer for `INT 21h AH=47h` and clears carry flag. In the current `piu_1st` flow, `\DATAS\BGA` is returned as `DATAS\BGA`.

## Observation

`piu_1st` passed the previous `INT 21h AH=47h` blocker. The next blocker is `CD 21` at relocated base + `0x000F423B`, with `EAX=0x00001900`.

This is `INT 21h AH=19h`, get current drive. The original appears to be building a drive letter plus current-directory path after querying the current directory string.

## Verification

* Passed: `cmd /c scripts\build_win32_x86.bat`
* Passed: `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
* Passed: `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1 -SkipSetup`
* Note: The Win32 x86 build still emits the existing third-party `spdlog` header `C4819` warning.
