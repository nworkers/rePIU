# DOS environment HLE 작업 로그

## 변경 내용

`piu_1st`의 DS low-memory 문자열 스캔이 더 이상 0으로 채워진 가짜 영역만 읽지 않도록, Win32 실행 trampoline에 DOS environment block HLE를 추가했다.

guest 실행 시작 전에 host process의 환경변수를 `NAME=VALUE\0...\0\0` 형식의 DOS environment block으로 직렬화한다. DOS 관례에 맞춰 변수 이름은 대문자로 정규화하고, 값은 host 값을 그대로 유지한다. 민감한 환경변수 값이 로그에 직접 노출되지 않도록 loader는 block 크기만 출력한다.

`ReadSegmentByte`와 `ReadSegmentDword`는 DS low-memory offset이 environment block 범위 안에 있을 때 해당 buffer에서 값을 읽는다. 범위를 벗어난 읽기는 기존 정책처럼 0을 반환한다.

실제 environment block을 제공하면 guest 코드가 timeout 이후에도 계속 실행되면서 loader 출력 중 process 상태를 흔들 수 있었다. timeout 관측값을 복사한 뒤 guest thread를 종료하도록 timeout 경로에 `TerminateThread` 호출을 추가했다. 이는 timeout 진단 실행을 안정화하기 위한 host-side 정리이며, 원본 guest 코드를 수정하지 않는다.

## 결과

`piu_1st`는 이제 실제 host 환경변수 기반 DOS environment block을 읽는다. 관측된 environment block 크기는 실행 환경에 따라 달라지며, 최근 검증에서는 약 3.5KB 범위로 출력되었다.

실제 environment scan이 진행되면서 현재 100,000회 single-step 진단 budget 안에서는 예전처럼 `chdir`/`open` 관측 지점까지 도달하지 않는다. 이는 파일 접근이 사라진 것이 아니라, 0-filled environment 응답 때보다 초기 environment scan 경로가 길어진 결과로 해석된다.

single-step budget을 1,000,000회로 늘리는 실험도 했지만, `piu_1st` 단독 실행이 30초를 넘겨 현재 자동 검증 흐름에는 맞지 않았다. 따라서 이번 작업에서는 budget을 기존 수준으로 유지하고, 다음 작업에서 시간 기반 또는 진행률 기반 진단 방식을 별도로 설계하는 편이 낫다.

## 검증

* `cmd /c scripts\build_win32_x86.bat`
  * 결과: 성공
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
  * 결과: 성공
  * 확인: DOS environment block 크기와 low-memory scan 관측 출력
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`
  * 결과: 성공

# DOS Environment HLE Work Log

## Changes

Added a DOS environment block HLE path to the Win32 execution trampoline so the `piu_1st` DS low-memory string scan no longer reads only zero-filled fake memory.

Before guest execution starts, the host process environment is serialized as a DOS environment block in `NAME=VALUE\0...\0\0` format. Variable names are normalized to uppercase to match DOS convention, while values are preserved as host values. To avoid exposing sensitive environment values directly, the loader prints only the block size.

`ReadSegmentByte` and `ReadSegmentDword` now read from the environment block when DS low-memory offsets fall inside the buffer. Reads outside the buffer still return `0`, matching the previous fallback policy.

With a real environment block, guest code could continue running after timeout and disturb the process while the loader was printing diagnostics. The timeout path now terminates the guest thread after copying timeout observations. This is host-side cleanup for diagnostic stability and does not modify original guest code.

## Result

`piu_1st` now reads a DOS environment block backed by the real host environment. The observed block size depends on the execution environment; recent verification printed a size around 3.5KB.

Because the real environment scan now makes progress through more data, the current 100,000 single-step diagnostic budget no longer reaches the previous `chdir`/`open` observation point. This does not mean file access disappeared; it means the initialization path is now spending the current diagnostic budget in environment scanning.

A 1,000,000 single-step budget was tested, but the standalone `piu_1st` run exceeded 30 seconds, so it is not suitable for the current automated verification flow. The better next step is to design a time-based or progress-based diagnostic strategy separately.

## Verification

* `cmd /c scripts\build_win32_x86.bat`
  * Result: passed
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
  * Result: passed
  * Checked: DOS environment block size and low-memory scan observations are printed
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`
  * Result: passed
