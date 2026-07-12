# DOS read size provenance 작업 로그 / Work Log

## 한국어

DOS read ring에 guest EIP, ESP, 상위 8개 stack dword를 추가했습니다. PIU.DAT의 마지막 large read에서 전체 크기 `0x00855C29`와 요청값 `0x00854D00`이 보존된 채 Watcom wrapper로 전달됨을 확인했습니다. 원본 wrapper와 loop는 `ECX/EAX`를 32비트로 사용했지만 HLE가 `CX/AX`로 축소한 것이 truncation의 원인이었습니다.

`AH=3Fh` HLE를 full `ECX/EAX` 규약으로 수정했습니다. Win32 x86 Debug 빌드는 성공했습니다. 수정 전에는 `Not PTX file`과 `exit(-1)`이 재현됐고, 수정 후 40초 동안 약 420만 dispatch가 진행되며 해당 오류와 종료가 나타나지 않았습니다.

## English

Extended the DOS read ring with guest EIP, ESP, and eight stack dwords. The final PIU.DAT large read proved that full size `0x00855C29` and request `0x00854D00` reached the Watcom wrapper intact. The original wrapper and loop use 32-bit `ECX/EAX`; truncation occurred only because HLE reduced them to `CX/AX`.

Changed `AH=3Fh` HLE to the full `ECX/EAX` convention. The Win32 x86 Debug build passed. Before the change, the run reproduced `Not PTX file` and `exit(-1)`; afterward, it remained live for 40 seconds and roughly 4.2 million dispatches without that error or termination.
