# INT 21h AH=3Eh 파일 close HLE 설계

`piu_1st`는 `AH=42h` file seek를 통과한 뒤 relocated base + `0x000F7A0C`의 `CD 21`에서 중단된다. 예외 시점 `EAX=0x00003E05`, `EBX=0x00000005`이므로 DOS `INT 21h AH=3Eh`, handle `0x0005` file close 요청으로 본다.

DOS `AH=3Eh`는 `BX`의 file handle을 닫는다. 성공 시 carry를 clear한다. 실패 시 carry를 set하고 `AX`에 DOS error code를 반환한다. 현재 VFS는 실제 host file descriptor를 계속 열어두지 않고, open handle table과 file offset만 유지하므로 close는 table entry를 닫힌 상태로 표시하는 작업이다.

HLE는 standard handle `0..4` close를 현재 범위 밖으로 둔다. 원본이 닫는 handle은 HLE가 할당한 `0x0005`이므로, open table에서 matching open handle을 찾으면 `open=false`로 바꾸고 성공 처리한다. 이미 닫혔거나 존재하지 않는 handle은 invalid handle `0x0006`으로 실패 처리한다.

검증은 `piu_1st`가 기존 `AH=3Eh` 지점을 통과하고 다음 관측 지점으로 이동하는지 확인한다. Win32 loader 로그에는 마지막 DOS close handle과 결과를 출력한다.

# INT 21h AH=3Eh File Close HLE Design

After passing `AH=42h` file seek, `piu_1st` stops at `CD 21` at relocated base + `0x000F7A0C`. With exception-time `EAX=0x00003E05` and `EBX=0x00000005`, this is treated as DOS `INT 21h AH=3Eh`, file close for handle `0x0005`.

DOS `AH=3Eh` closes the file handle in `BX`. On success it clears carry. On failure it sets carry and returns a DOS error code in `AX`. The current VFS does not keep a host file descriptor open; it only keeps an open handle table and file offset, so close marks the table entry closed.

Standard handle `0..4` close is out of scope for this step. The observed original request closes HLE-allocated handle `0x0005`, so finding a matching open table entry marks it `open=false` and succeeds. Missing or already closed handles fail with invalid handle `0x0006`.

Verification checks that `piu_1st` passes the previous `AH=3Eh` point and reaches the next observed point. The Win32 loader log prints the last DOS close handle and result.
