# DOS Resize Guard 작업 로그

## 결과

`INT 21h AH=0x4A` 요청의 selector, paragraph, 결과를 loader 로그에 남기도록 했다. 단순 arena slack 증가는 대상 주소가 새 arena 끝 뒤로 함께 이동해 효과가 없음을 확인했고, 관측된 `ES=0x0024`, `BX=0xE7E1` 요청에는 임시 상한 `0xE700`을 적용해 insufficient memory로 실패시키도록 했다.

이 변경 후 `piu_1st`는 `stage.cfg` open 시도까지 진행했다. 현재 다음 관측 지점은 `0x020F7340`의 `C7 01 FF FF FF FF` 일반 memory write이며, 마지막 resize 요청은 `ES=0x0024`, `BX=0x4AE1` 성공이다.

## 검증

* `cmd /c scripts\build_win32_x86.bat`: 성공
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`: `stage.cfg` open 시도와 현재 `0x020F7340` blocker 확인

# DOS Resize Guard Work Log

## Result

The loader now logs selector, paragraph count, and result for `INT 21h AH=0x4A` requests. A simple arena slack increase was tested and shown not to solve the issue because the target address moved along with the new arena end. The observed `ES=0x0024`, `BX=0xE7E1` request now uses a temporary `0xE700` limit and fails with insufficient memory.

After this change, `piu_1st` advances to a `stage.cfg` open attempt. The current next observed point is a normal memory write `C7 01 FF FF FF FF` at `0x020F7340`; the last resize request is a successful `ES=0x0024`, `BX=0x4AE1`.

## Verification

* `cmd /c scripts\build_win32_x86.bat`: passed
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`: confirmed the `stage.cfg` open attempt and the current blocker at `0x020F7340`
