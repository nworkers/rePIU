# Arena 외부 allocator sentinel store 작업 로그

## 결과

`piu_1st`가 `intro.ani`를 닫은 뒤 relocated base + `0x000F86E0`에서 수행하는 `C7 01 FF FF FF FF`를 제한된 allocator 실패 sentinel store로 처리했다.

처음에는 runtime arena slack을 1 MiB에서 2 MiB로 늘리는 방법을 검증했다. 단독 실행은 더 진행했지만 전체 테스트에서는 guest가 추가 공간을 사용한 뒤 같은 `C7` 명령이 새 arena 끝 바깥에서 다시 중단되었다. 따라서 고정 slack 확장은 되돌렸다.

기존 memory-store HLE에 다음 조건을 추가했다.

* `C7 /0` dword immediate store
* 값 `0xFFFFFFFF`
* destination이 runtime arena end 이상이고 end 이후 1 MiB 미만

조건을 만족하는 store만 shadow memory에 기록한다. 기존 파일 열기 실패 경로와 실제 arena 내부 store 정책은 변경하지 않았다.

## 관측

기존 `C7` 중단점은 통과했고 다음 blocker는 relocated base + `0x000F7AA8`의 `89 17`이다. 예외 시점의 주요 상태는 `EDI=0x026E49C4`, `EDX=0x00000490`이며 명령은 `mov dword ptr [edi], edx`로 해석된다.

관측된 sentinel store는 다음과 같다.

* instruction: relocated base + `0x000F86E0`
* destination: `0x026E4E54`
* value: `0xFFFFFFFF`
* applied to real arena: `false`
* shadow memory write count: `1`
* shadow memory read hit count: `1`

## 검증

* `cmd /c scripts\build_win32_x86.bat`
  * 성공
  * 기존 third-party `spdlog` code-page 경고 `C4819`만 출력됨
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
  * 성공적으로 기존 `C7`을 통과하고 새 `89 17` blocker 관측
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1 -SkipSetup`
  * 성공

# Out-of-Arena Allocator Sentinel Store Work Log

## Result

Handled the `C7 01 FF FF FF FF` instruction at relocated base + `0x000F86E0`, reached after `piu_1st` closes `intro.ani`, as a constrained allocator failure sentinel store.

An initial experiment increased runtime arena slack from 1 MiB to 2 MiB. A standalone run advanced, but the full test consumed the extra space and stopped on the same `C7` instruction beyond the new arena end. The fixed-slack increase was therefore reverted.

The existing memory-store HLE now accepts only stores matching all of these conditions:

* `C7 /0` dword immediate store
* value `0xFFFFFFFF`
* destination at or above the runtime arena end and less than 1 MiB after it

Only a matching store is recorded in shadow memory. Existing file-open-failure behavior and stores inside the real arena are unchanged.

## Observation

The previous `C7` stop is passed. The next blocker is `89 17` at relocated base + `0x000F7AA8`. The key exception-time state is `EDI=0x026E49C4` and `EDX=0x00000490`, so the instruction decodes as `mov dword ptr [edi], edx`.

The observed sentinel store is:

* instruction: relocated base + `0x000F86E0`
* destination: `0x026E4E54`
* value: `0xFFFFFFFF`
* applied to real arena: `false`
* shadow memory write count: `1`
* shadow memory read hit count: `1`

## Verification

* `cmd /c scripts\build_win32_x86.bat`
  * Passed
  * Only the existing third-party `spdlog` code-page warning `C4819` was reported
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
  * Passed the previous `C7` and observed the new `89 17` blocker
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1 -SkipSetup`
  * Passed
