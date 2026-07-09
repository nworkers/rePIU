# Traced DOS Write HLE 작업 로그

## 결과

traced DOS HLE에 `INT 21h AH=0x40` 처리를 추가했다. `piu_1st`는 handle `1`, 길이 `10`의 write 요청을 수행했고, 출력 버퍼는 HLE console output에 누적되어 `--- FAIL` 메시지로 확인되었다.

## 검증

* `cmd /c scripts\build_win32_x86.bat`: 성공
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`: 이전 `0x020F6476` write 지점을 통과하고 HLE console output 10바이트를 기록

# Traced DOS Write HLE Work Log

## Result

Added `INT 21h AH=0x40` handling to traced DOS HLE. `piu_1st` issued a write to handle `1` with length `10`; the buffer was appended to HLE console output and appeared as the `--- FAIL` message.

## Verification

* `cmd /c scripts\build_win32_x86.bat`: passed
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`: passed the previous write point at `0x020F6476` and recorded 10 bytes of HLE console output
