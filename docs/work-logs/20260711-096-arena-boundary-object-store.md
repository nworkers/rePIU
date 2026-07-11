# Arena 경계 객체 memory store 작업 로그

## 결과

실제 arena 내부 마지막 64바이트에서 시작하고 최대 64바이트만 경계를 넘는 객체의 field store를 shadow memory로 보존했다.

지원한 기존 decoder는 다음과 같다.

* `C7 /0` immediate dword store
* `89 /r` register dword store
* `D9 /2-/3` FPU m32 store

base register가 arena 내부에 있어야 하며, base와 destination 모두 64바이트 경계 창 조건을 만족해야 한다. 일반적인 arena 외부 객체는 허용하지 않는다.

## 관측

`EAX=arena end-4`인 객체의 `+0x0C`, `+0x10`, `+0x14`, `+0x18`, `+0x20`, `+0x24`, `+0x28` 필드 흐름이 진행됐다. shadow read hit도 `6`회 발생해 기록된 tail이 후속 초기화에 사용됨을 확인했다.

* handled memory store count: `14`
* last handled store: relocated base + `0x0001E182`
* shadow write count: `14`
* shadow read hit count: `6`
* shadow byte count: `56`

다음 blocker는 relocated base + `0x0001E141`의 `66 C7 00 00 00`이다. 이때 `EAX=arena end+0x28`로 새 객체 base 자체가 arena 밖이다. 이는 이번 “arena 내부 base의 경계 tail” 정책에 포함되지 않으며, 다음 작업에서 allocator가 arena 밖 객체를 계속 반환하는 이유와 word-store 처리 방향을 별도로 분석해야 한다.

## 검증

* `cmd /c scripts\build_win32_x86.bat`: 성공
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`: 경계 객체 필드 통과 및 새 blocker 관측
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1 -SkipSetup`: 성공

# Arena-Boundary Object Memory Store Work Log

## Result

Preserved field stores through shadow memory for an object whose base starts within the final 64 bytes of the real arena and crosses no more than 64 bytes beyond its end.

The existing supported decoders are:

* `C7 /0` immediate dword store
* `89 /r` register dword store
* `D9 /2-/3` FPU m32 store

The base register must remain inside the arena, and both base and destination must satisfy the 64-byte boundary-window conditions. General objects outside the arena are not accepted.

## Observation

The object with `EAX=arena end-4` progressed through fields at `+0x0C`, `+0x10`, `+0x14`, `+0x18`, `+0x20`, `+0x24`, and `+0x28`. Six shadow read hits confirm that the stored tail is consumed by following initialization.

* handled memory store count: `14`
* last handled store: relocated base + `0x0001E182`
* shadow write count: `14`
* shadow read hit count: `6`
* shadow byte count: `56`

The next blocker is `66 C7 00 00 00` at relocated base + `0x0001E141`. At this point `EAX=arena end+0x28`, so the new object base itself is outside the arena. This is outside the current boundary-tail policy and requires separate analysis of why the allocator continues returning out-of-arena objects and how the word store should be handled.

## Verification

* `cmd /c scripts\build_win32_x86.bat`: passed
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`: passed boundary-object fields and observed the new blocker
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1 -SkipSetup`: passed
