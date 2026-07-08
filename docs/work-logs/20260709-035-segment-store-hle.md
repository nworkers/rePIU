# segment register store HLE 작업 로그

## 결과

`66 26 8C 1D ED 3A 0F 02` segment register store를 guest selector shadow state와 relocated runtime memory write HLE로 처리했다.

구현 내용은 다음과 같다.

* privileged instruction classifier가 instruction prefix를 건너뛰고 실제 opcode를 분류하도록 확장했다.
* `8C /r`를 `MOV r/m16, Sreg` CPU/DPMI state initialization candidate로 분류했다.
* Win32 execution trampoline에 segment store 처리 count와 마지막 처리 정보를 추가했다.
* `66 26 8C /r` 중 absolute 32-bit displacement memory destination 형태를 디코딩했다.
* guest segment shadow state에서 selector를 읽고, relocated runtime memory에 16-bit 값으로 기록했다.
* relocated object 보호 속성 때문에 HLE write 구간에서만 `VirtualProtect`로 2바이트 쓰기를 허용한 뒤 보호 속성을 복구했다.
* loader 로그에 segment store 처리 정보를 출력했다.

## 검증

`cmd /c scripts\build_win32_x86.bat`

결과: 성공.
기존과 동일하게 spdlog 외부 header의 MSVC C4819 경고가 남아 있지만 빌드는 성공했다.

`build\vs2022_win32_debug\Debug\repiu_loader_win32.exe piu_1st`

결과:

* process exit code: 0
* handled HLE trap count: 1
* handled DOS interrupt count: 2
* handled segment load count: 2
* handled segment store count: 1
* last segment store address: `0x020F39B2`
* last stored segment register: `DS`
* last stored segment selector: `0x0024`
* last segment store destination: `0x020F3AED`
* 다음 중단 지점: `0x020F39C8`
* 다음 byte window focus: `66 8E 05 E4 65 1A 02`

`build\vs2022_win32_debug\Debug\repiu_loader_win32.exe dos4gw_hello`

결과:

* process exit code: 0
* stdout: `Hello, world!`
* HLE console output bytes: 15
* original entry returned to host trampoline

## 다음 작업

다음 중단 지점은 `66 8E 05 E4 65 1A 02`이다.
이는 prefix가 붙은 `MOV Sreg, r/m16` memory-source 형태이므로, guest selector shadow state를 memory source와 연결하는 segment load HLE 확장이 필요하다.

# Segment Register Store HLE Work Log

## Result

Handled `66 26 8C 1D ED 3A 0F 02` segment-register store through guest selector shadow state and relocated runtime memory write HLE.

Implemented changes:

* Extended the privileged instruction classifier to skip instruction prefixes before classifying the real opcode.
* Classified `8C /r` as `MOV r/m16, Sreg`, a CPU/DPMI state initialization candidate.
* Added segment store handled count and last-handled fields to the Win32 execution trampoline.
* Decoded the absolute 32-bit displacement memory destination form of `66 26 8C /r`.
* Read the selector from guest segment shadow state and wrote it as a 16-bit value into relocated runtime memory.
* Temporarily used `VirtualProtect` around the 2-byte HLE write because relocated object protection can block direct writes.
* Printed segment store handling information in loader logs.

## Verification

`cmd /c scripts\build_win32_x86.bat`

Result: success.
The existing MSVC C4819 warning from an external spdlog header remains, but the build succeeds.

`build\vs2022_win32_debug\Debug\repiu_loader_win32.exe piu_1st`

Result:

* process exit code: 0
* handled HLE trap count: 1
* handled DOS interrupt count: 2
* handled segment load count: 2
* handled segment store count: 1
* last segment store address: `0x020F39B2`
* last stored segment register: `DS`
* last stored segment selector: `0x0024`
* last segment store destination: `0x020F3AED`
* next stop: `0x020F39C8`
* next byte-window focus: `66 8E 05 E4 65 1A 02`

`build\vs2022_win32_debug\Debug\repiu_loader_win32.exe dos4gw_hello`

Result:

* process exit code: 0
* stdout: `Hello, world!`
* HLE console output bytes: 15
* original entry returned to host trampoline

## Next Work

The next stop is `66 8E 05 E4 65 1A 02`.
This is a prefixed `MOV Sreg, r/m16` memory-source form, so segment load HLE needs to be extended to read selectors from memory sources.
