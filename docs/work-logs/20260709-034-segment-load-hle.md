# segment register load HLE 작업 로그

## 결과

privileged instruction classifier에 `0x8E` (`MOV Sreg, r/m16`) 분류를 추가했다.
이 명령은 일반 HLE trap이 아니라 selector/descriptor 상태 HLE 요구사항으로 분류한다.

Win32 execution trampoline에는 segment load HLE 경로를 추가했다.
`8E /r` 중 register source 형태를 디코딩해 source register의 low 16-bit 값을 guest selector로 읽고, target segment register와 selector를 host CPU segment register가 아닌 `ThreadContext`의 guest segment shadow state에 기록한다.

현재 처리한 명령은 다음과 같다.

* `8E D9`: `MOV DS, CX`
* 이후 관찰된 register-source segment load 1개

execution attempt에는 처리 count, 마지막 처리 EIP, opcode, segment register, selector를 기록한다.

## 검증

* `cmd /c scripts\build_win32_x86.bat`: 성공
  * `spdlog` 외부 헤더의 MSVC C4819 경고는 유지되지만 빌드는 성공했다.
* `build\vs2022_win32_debug\Debug\repiu_loader_win32.exe piu_1st`: 성공
  * handled segment load count: `2`
  * last handled segment load address: `0x020F39B0`
  * last handled segment load opcode: `0x8E`
  * last handled segment register: `ES`
  * last handled segment selector: `0x0017`
  * 기존 `0x020F3995`의 `8E D9`를 통과한 뒤 다음 중단 지점 `0x020F39B2`에 도달했다.
  * 다음 byte window는 `66 26 8C 1D ...`를 포함한다.
* `build\vs2022_win32_debug\Debug\repiu_loader_win32.exe dos4gw_hello`: 성공
  * stdout의 `Hello, world!` 출력 유지
  * handled segment load count: `0`

## 판단

`MOV Sreg, r/m16` register-source 형태는 guest selector shadow state로 기록하고 건너뛰는 최소 HLE로 통과 가능함이 확인되었다.
다음 중단 지점은 segment register 값을 memory에 저장하는 형태로 보인다.
따라서 다음 작업은 `66 26 8C 1D ...`를 디코딩하고, guest selector shadow state의 값을 relocated memory에 쓰는 정책을 설계하는 것이다.

# Segment Register Load HLE Work Log

## Result

Added `0x8E` (`MOV Sreg, r/m16`) classification to the privileged instruction classifier.
This instruction is classified as a selector/descriptor state HLE requirement, not as a normal HLE trap.

Added a segment load HLE path to the Win32 execution trampoline.
For `8E /r` register-source forms, it decodes the source register low 16-bit value as a guest selector, then records the target segment register and selector in `ThreadContext` guest segment shadow state instead of changing the host CPU segment register.

The currently handled instructions include:

* `8E D9`: `MOV DS, CX`
* one later register-source segment load observed in the same path

The execution attempt records handled count, last handled EIP, opcode, segment register, and selector.

## Verification

* `cmd /c scripts\build_win32_x86.bat`: passed
  * MSVC C4819 warnings from external `spdlog` headers remain, but the build passed.
* `build\vs2022_win32_debug\Debug\repiu_loader_win32.exe piu_1st`: passed
  * handled segment load count: `2`
  * last handled segment load address: `0x020F39B0`
  * last handled segment load opcode: `0x8E`
  * last handled segment register: `ES`
  * last handled segment selector: `0x0017`
  * execution passed the previous `8E D9` stop at `0x020F3995` and reached the next stop at `0x020F39B2`
  * the next byte window includes `66 26 8C 1D ...`
* `build\vs2022_win32_debug\Debug\repiu_loader_win32.exe dos4gw_hello`: passed
  * Preserved `Hello, world!` on stdout
  * handled segment load count: `0`

## Judgment

`MOV Sreg, r/m16` register-source forms can pass through minimal HLE by recording guest selector shadow state.
The next stop appears to store a segment register value to memory.
The next task is to decode `66 26 8C 1D ...` and design the policy for writing guest selector shadow state into relocated memory.
