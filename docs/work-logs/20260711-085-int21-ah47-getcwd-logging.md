# INT 21h AH=47h 현재 디렉터리 조회 로그 작업 로그

## 결과

`INT 21h AH=47h` 처리 결과를 Win32 loader 로그에 직접 출력하도록 관측 필드를 추가했다.

추가된 로그 항목은 다음과 같다.

* `Win32 handled DOS getcwd count`
* `Win32 last DOS getcwd drive`
* `Win32 last DOS getcwd path`
* `Win32 last DOS getcwd result`
* 실패 시 `Win32 last DOS getcwd error`

## 관측

`piu_1st` 실행에서 `AH=47h`가 반환한 현재 디렉터리가 로그에 직접 보인다.

* drive: `0x00`
* path: `DATAS\BGA`
* result: `success`

다음 중단점은 기존과 동일하게 `INT 21h AH=19h` 현재 drive 조회이다.

## 검증

* 통과: `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1 -SkipSetup`
* 참고: Win32 x86 빌드 중 서드파티 `spdlog` 헤더의 기존 `C4819` 경고가 출력되었다.

# INT 21h AH=47h Getcwd Logging Work Log

## Result

Observation fields were added so the `INT 21h AH=47h` result is printed directly in the Win32 loader log.

The added log fields are:

* `Win32 handled DOS getcwd count`
* `Win32 last DOS getcwd drive`
* `Win32 last DOS getcwd path`
* `Win32 last DOS getcwd result`
* `Win32 last DOS getcwd error` on failure

## Observation

The current directory returned by `AH=47h` is now directly visible in the `piu_1st` log.

* drive: `0x00`
* path: `DATAS\BGA`
* result: `success`

The next blocker remains `INT 21h AH=19h`, get current drive.

## Verification

* Passed: `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1 -SkipSetup`
* Note: The Win32 x86 build still emits the existing third-party `spdlog` header `C4819` warning.
