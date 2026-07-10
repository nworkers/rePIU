# INT 21h AH=3Eh 파일 close HLE 작업 로그

## 결과

DOS VFS에 open handle 기반 close helper를 추가했다. HLE가 할당한 열린 handle을 찾으면 table entry를 `open=false`로 바꾸고 성공 처리한다. 이미 닫혔거나 존재하지 않는 handle은 invalid handle `0x0006`으로 실패 처리한다.

Win32 execution trampoline은 `INT 21h AH=3Eh`를 일반 DOS interrupt handler와 traced DOS interrupt handler 양쪽에서 처리한다. 성공 시 carry flag를 clear하고, 실패 시 carry flag를 set한 뒤 `AX`에 DOS error code를 반환한다.

Win32 loader 로그에는 마지막 DOS close handle과 결과를 출력하도록 했다.

## 관찰

`piu_1st`는 `intro.ani` handle `0x0005`를 `AH=3Eh`로 닫았고, close는 성공했다.

이 작업 후 기존 `AH=3Eh` 중단점은 통과했다. 다음 blocker는 relocated base + `0x000F86E0`의 opcode `0xC7`이며, byte window는 `C7 01 FF FF FF FF`를 포함한다. 이는 현재 classifier/메모리 store HLE가 아직 처리하지 않는 guest memory store 후보이다.

## 검증

다음 명령으로 검증했다.

* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1 -SkipSetup`

결과는 통과했다. Win32 x86 빌드 중 third-party `spdlog` header의 기존 코드 페이지 경고 `C4819`가 계속 출력되지만 빌드 실패는 아니다.

# INT 21h AH=3Eh file close HLE work log

## Result

Added an open-handle-based close helper to the DOS VFS. If an HLE-allocated open handle is found, the table entry is marked `open=false` and the call succeeds. Missing or already closed handles fail with invalid handle `0x0006`.

The Win32 execution trampoline handles `INT 21h AH=3Eh` in both the normal DOS interrupt handler and the traced DOS interrupt handler. On success it clears carry. On failure it sets carry and returns a DOS error code in `AX`.

The Win32 loader log now prints the last DOS close handle and result.

## Observation

`piu_1st` closed `intro.ani` handle `0x0005` through `AH=3Eh`, and the close succeeded.

After this task, the previous `AH=3Eh` blocker is passed. The next blocker is opcode `0xC7` at relocated base + `0x000F86E0`, and the byte window contains `C7 01 FF FF FF FF`. This is a guest memory store candidate not yet handled by the current classifier/memory-store HLE.

## Verification

Verified with:

* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1 -SkipSetup`

The verification passed. The Win32 x86 build still reports the existing third-party `spdlog` code-page warning `C4819`, but it is not a build failure.
