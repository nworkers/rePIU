# DOS 종료 provenance 작업 로그 / Work Log

## 한국어

`AH=4Ch`에서 AX/EIP/ESP와 stack 128 dword를 capture하도록 구현했습니다. Win32 x86 Debug 빌드와 PIU 실행이 성공했습니다.

종료 stack은 `+0xDDD2B` memory PTX loader, `+0xDF884` runtime error path, `+0xE52D8` exit cleanup을 확인하여 `Not PTX file` error printer가 `exit(-1)`로 이어짐을 증명했습니다. PTX 입력 pointer는 `0x03BB6AE9`, caller는 `+0xE1DC9`입니다. frame local에 복사된 16-byte header는 zero였습니다.

상위 frame 문자열은 `hfont1.tga`, `hfont2.tga`이고 archive의 대응 entry `HFONT1.PTX`, `HFONT2.PTX`는 정상 header를 가집니다. pointer는 archive buffer base `0x0393B650`에 entry absolute offset `0x27B499`를 더한 정확한 값입니다. 그러나 archive payload size `0x00855C29` 중 low 16-bit에 해당하는 약 `0x5C00`만 읽혀 해당 pointer는 zero-filled reserve를 가리킵니다.

## English

Implemented capture of AX/EIP/ESP and 128 stack dwords at DOS `AH=4Ch`; the Win32 x86 Debug build and PIU run succeeded.

The stack identifies memory PTX loader `+0xDDD2B`, runtime error path `+0xDF884`, and exit cleanup `+0xE52D8`, proving that the `Not PTX file` error printer reaches `exit(-1)`. The PTX input is `0x03BB6AE9`, called from `+0xE1DC9`, and its copied 16-byte header is zero. The pointer correctly equals archive buffer base plus the valid `HFONT1.PTX` absolute offset, but only the low-16-bit-sized portion of the `0x00855C29` payload was read.
