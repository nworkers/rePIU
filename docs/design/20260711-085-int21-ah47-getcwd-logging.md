# INT 21h AH=47h 현재 디렉터리 조회 로그 설계

## 배경

`INT 21h AH=47h` 처리는 추가되었지만, 실행 로그에는 마지막 DOS interrupt가 `AX=0x4700`이었다는 간접 정보만 남는다. 실제 guest 버퍼에 기록한 현재 디렉터리 문자열은 로그에서 확인할 수 없다.

## 설계

`chdir`/`open` 관측 로그와 같은 방식으로 `getcwd` 전용 관측 필드를 추가한다.

* `ThreadContext`에 `handled_dos_getcwd_count`, 마지막 drive, path, 성공 여부, 오류 코드를 기록한다.
* `Win32MinimalExecutionAttempt`에 같은 필드를 추가하고 execution 결과 복사 시 전달한다.
* Win32 loader 로그에 `Win32 handled DOS getcwd count`, 마지막 drive/path/result/error를 출력한다.
* `scripts/test_all.ps1`에서 `DATAS\BGA` 반환 로그를 검증한다.

## 범위 밖

* `AH=47h` 의미 변경
* `AH=19h` 현재 drive 조회 구현

# INT 21h AH=47h Getcwd Logging Design

## Background

`INT 21h AH=47h` handling was added, but the execution log only shows the indirect fact that the last DOS interrupt was `AX=0x4700`. The current-directory string written to the guest buffer is not visible in the log.

## Design

Add dedicated `getcwd` observation fields following the existing `chdir`/`open` logging pattern.

* Record `handled_dos_getcwd_count`, last drive, path, success flag, and error code in `ThreadContext`.
* Add the same fields to `Win32MinimalExecutionAttempt` and copy them from the execution result.
* Print `Win32 handled DOS getcwd count`, last drive/path/result/error in the Win32 loader log.
* Verify the `DATAS\BGA` return path in `scripts/test_all.ps1`.

## Out Of Scope

* Changing `AH=47h` semantics
* Implementing `AH=19h` get current drive
