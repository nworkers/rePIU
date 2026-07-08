# memory-source segment register load HLE 작업 로그

## 결과

`66 8E 05 E4 65 1A 02` memory-source segment register load를 relocated memory read HLE로 처리했다.

구현 내용은 다음과 같다.

* segment load HLE가 instruction prefix를 건너뛰고 `8E /r`를 디코딩하도록 확장했다.
* 기존 `mod=3` register-source segment load 경로를 유지했다.
* `mod=00`, `r/m=5` absolute displacement memory-source 경로를 추가했다.
* relocated runtime memory에서 16-bit selector를 읽어 guest segment shadow state에 기록했다.
* execution attempt와 loader 로그에 마지막 segment load source address를 추가했다.

## 검증

`cmd /c scripts\build_win32_x86.bat`

결과: 성공.
기존과 동일하게 spdlog 외부 header의 MSVC C4819 경고가 남아 있지만 빌드는 성공했다.

`build\vs2022_win32_debug\Debug\repiu_loader_win32.exe piu_1st`

결과:

* process exit code: 0
* handled HLE trap count: 1
* handled DOS interrupt count: 2
* handled segment load count: 3
* last segment load address: `0x020F39C8`
* last segment load register: `ES`
* last segment load selector: `0x0024`
* last segment load source: `0x021A65E4`
* handled segment store count: 1
* 다음 중단 지점: `0x020F39DD`
* 다음 byte window focus: `26 8A 4F FF`

`build\vs2022_win32_debug\Debug\repiu_loader_win32.exe dos4gw_hello`

결과:

* process exit code: 0
* stdout: `Hello, world!`
* HLE console output bytes: 15
* original entry returned to host trampoline

## 다음 작업

다음 중단 지점은 `26 8A 4F FF`이다.
이는 segment override가 붙은 byte memory load 형태이며, 현재 selector shadow state와 low-memory 또는 descriptor 기반 주소 변환 정책을 더 명확히 해야 한다.

# Memory-Source Segment Register Load HLE Work Log

## Result

Handled `66 8E 05 E4 65 1A 02` memory-source segment-register load through relocated memory read HLE.

Implemented changes:

* Extended segment load HLE to skip instruction prefixes before decoding `8E /r`.
* Preserved the existing `mod=3` register-source segment load path.
* Added the `mod=00`, `r/m=5` absolute displacement memory-source path.
* Read a 16-bit selector from relocated runtime memory and recorded it in guest segment shadow state.
* Added the last segment load source address to execution attempts and loader logs.

## Verification

`cmd /c scripts\build_win32_x86.bat`

Result: success.
The existing MSVC C4819 warning from an external spdlog header remains, but the build succeeds.

`build\vs2022_win32_debug\Debug\repiu_loader_win32.exe piu_1st`

Result:

* process exit code: 0
* handled HLE trap count: 1
* handled DOS interrupt count: 2
* handled segment load count: 3
* last segment load address: `0x020F39C8`
* last segment load register: `ES`
* last segment load selector: `0x0024`
* last segment load source: `0x021A65E4`
* handled segment store count: 1
* next stop: `0x020F39DD`
* next byte-window focus: `26 8A 4F FF`

`build\vs2022_win32_debug\Debug\repiu_loader_win32.exe dos4gw_hello`

Result:

* process exit code: 0
* stdout: `Hello, world!`
* HLE console output bytes: 15
* original entry returned to host trampoline

## Next Work

The next stop is `26 8A 4F FF`.
This is a byte memory load with a segment override, and the selector shadow state plus low-memory or descriptor-based address translation policy should be clarified next.
