# Allocator shadow metadata 89 store 작업 로그

## 결과

기존 allocator sentinel과 구조적으로 연결된 `89 /r` out-of-arena store를 shadow memory로 처리했다.

첫 `89 17` store는 destination `0x026E49C4`에서 값 `0x00000490`을 기록한다. 기존 sentinel `0x026E4E54`와 destination의 차이가 값과 정확히 일치하는 경우만 새 allocator header의 시작으로 허용했다. 이후 store는 확장된 shadow 범위 내부 또는 바로 다음 dword에 한정했다.

## 관측

`89 17`과 인접 header field store가 통과했다.

* handled memory store count: `4`
* last handled store: relocated base + `0x000F7AB2`
* last opcode/source: `0x89`, `mov-reg32`
* shadow write count: `4`
* shadow byte count: `16`
* shadow range: `0x026E49C4` .. `0x026E4E57`

다음 blocker는 relocated base + `0x0001E14C`의 `C7 40 18 00 00 00 00`이다. 예외 시점 `EAX=0x026D6FFC`이며 destination `EAX+0x18`은 `0x026D7014`이다. 객체 base는 arena 끝 `0x026D7000`의 4바이트 앞에 있지만 필드가 경계를 넘어간다. 이는 allocator shadow metadata 연결과 다른 arena-boundary object initialization 문제이므로 다음 작업에서 별도로 설계해야 한다.

## 검증

* `cmd /c scripts\build_win32_x86.bat`
  * 성공
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
  * 기존 `89 17` 및 인접 store 통과, 새 `C7 40 18` blocker 관측
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1 -SkipSetup`
  * 성공

# Allocator Shadow Metadata 89 Store Work Log

## Result

Handled out-of-arena `89 /r` stores structurally connected to the existing allocator sentinel through shadow memory.

The first `89 17` store writes value `0x00000490` at destination `0x026E49C4`. It is accepted as the start of a new allocator header only because the difference between the existing sentinel `0x026E4E54` and the destination exactly matches the stored value. Following stores are constrained to the expanded shadow range or its immediately adjacent next dword.

## Observation

The `89 17` instruction and adjacent header-field stores are passed.

* handled memory store count: `4`
* last handled store: relocated base + `0x000F7AB2`
* last opcode/source: `0x89`, `mov-reg32`
* shadow write count: `4`
* shadow byte count: `16`
* shadow range: `0x026E49C4` .. `0x026E4E57`

The next blocker is `C7 40 18 00 00 00 00` at relocated base + `0x0001E14C`. Exception-time `EAX=0x026D6FFC`, so destination `EAX+0x18` is `0x026D7014`. The object base is four bytes before arena end `0x026D7000`, but its field crosses the boundary. This differs from allocator shadow-metadata linkage and requires a separate design for arena-boundary object initialization.

## Verification

* `cmd /c scripts\build_win32_x86.bat`
  * Passed
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
  * Passed the previous `89 17` and adjacent stores; observed the new `C7 40 18` blocker
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1 -SkipSetup`
  * Passed
