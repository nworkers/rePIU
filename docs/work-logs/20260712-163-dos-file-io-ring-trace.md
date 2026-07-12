# DOS file I/O ring trace 작업 로그 / Work Log

## 한국어

read/seek 공용 sequence를 갖는 64-entry ring을 추가했습니다. 각 read는 handle, host path, 요청/실제 크기, 이전/이후 offset과 선두 16바이트를 기록하고 seek는 origin, signed offset, 결과 위치와 DOS 오류를 기록합니다.

Win32 x86 Debug 빌드와 PIU 실행이 성공했습니다. 34개 I/O가 기록되어 wrap은 없었습니다. `PIU.DAT` handle 9의 주요 결과는 다음과 같습니다.

```text
read 0x0000..0x1000 prefix 52 45 53 00 (RES)
read 0x1000..0x2A00 success
read 0x2A00..0x3A00 success
read 0x3A00..0x8600 success
seek current 0x8600 success
```

정적 자산 확인에서 첫 embedded PTX는 absolute `0x2BA8`, 두 번째는 `0x6BFC`에서 `50 54 58 00` magic을 가진다. 둘 다 성공적으로 읽힌 `0x0000..0x8600` 범위 안이다. 따라서 `ERROR: Not PTX file`은 DOS read/seek 실패가 아니라 메모리 내 RES entry 선택 또는 PTX buffer pointer/offset 계산 이후에 발생한다. 마지막 handle 4..0 seek의 error 6은 오류 메시지 뒤 C runtime 종료 정리에서 발생한다.

## English

Added a 64-entry ring with one sequence for reads and seeks. Reads record handle, host path, requested/actual size, before/after offsets, and a 16-byte prefix. Seeks record origin, signed offset, resulting position, and DOS error.

The Win32 x86 Debug build and PIU execution succeeded. All 34 operations fit without wrapping. Handle 9 read `PIU.DAT` successfully from offset 0 through `0x8600`, beginning with `RES`. Static asset inspection found embedded `PTX\0` magic at absolute offsets `0x2BA8` and `0x6BFC`, both inside the successfully read range. Therefore `ERROR: Not PTX file` occurs after DOS I/O, during in-memory RES entry selection or PTX buffer pointer/offset calculation. Invalid-handle seeks on handles 4 through 0 belong to C-runtime shutdown after the message.
