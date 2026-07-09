# segment override byte memory load HLE 작업 로그

## 결과

`piu_1st`의 기존 current blocker였던 `26 8A 4F FF`를 segment override byte memory load HLE로 처리했다.

구현 내용은 다음과 같다.

* Win32 execution trampoline에 `HandleSegmentOverrideByteLoadInstruction`을 추가했다.
* 현재 관찰된 `26 8A 4F FF`만 제한적으로 처리한다.
* 명령을 `mov cl, byte ptr es:[edi - 1]`로 해석한다.
* guest `ES` shadow selector와 `EDI - 1` offset을 사용한다.
* 현재 관찰된 `ES=0x0024`, offset `0x80`은 DOS command tail length byte로 보고 `0x00`을 반환한다.
* `CL`에 반환 값을 기록하고 `EIP`를 4만큼 진행시킨다.
* execution attempt와 loader 로그에 segment memory load 처리 횟수, 마지막 address, selector, offset, value를 추가했다.
* `scripts/test_all.ps1`의 `piu_1st` 검증 기준을 새 관찰 지점으로 갱신했다.
* `ARCHITECTURE.md`와 `docs/TODO.md`에 현재 처리 범위와 다음 중단 지점을 기록했다.

## 검증

`powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`

결과: 성공.

`dos4gw_hello`:

* `Hello, world!` 출력 유지.
* process exit code 0.

`piu_1st`:

* 기존 `26 8A 4F FF` 지점을 통과했다.
* handled segment memory load count: 1
* last segment memory load address: `0x020F4D7D`
* last segment memory load register: `ES`
* last segment memory load selector: `0x0024`
* last segment memory load offset: `0x00000080`
* last segment memory load value: `0x00`
* 다음 중단 지점: `0x020F4DAC`
* 다음 byte-window focus: `[8B] 06`
* 직전 segment load: `DS=0x002C`, source `0x021A664D`

기존과 동일하게 외부 `spdlog` header에서 MSVC C4819 경고가 발생할 수 있으나 빌드는 성공한다.

## 다음 작업

다음 중단 지점은 `8B 06`이다.
예외 시점에서 `DS=0x002C`, `ESI=0x00000000`이므로 DS 기반 low-memory 또는 descriptor 기반 32-bit memory read 정책을 명확히 해야 한다.

# Segment Override Byte Memory Load HLE Work Log

## Result

Handled the previous `piu_1st` current blocker, `26 8A 4F FF`, as a segment override byte memory load HLE.

Implemented changes:

* Added `HandleSegmentOverrideByteLoadInstruction` to the Win32 execution trampoline.
* Limited handling to the currently observed `26 8A 4F FF` form.
* Decoded the instruction as `mov cl, byte ptr es:[edi - 1]`.
* Used the guest `ES` shadow selector and `EDI - 1` offset.
* Treated the currently observed `ES=0x0024`, offset `0x80` as the DOS command tail length byte and returned `0x00`.
* Wrote the returned value into `CL` and advanced `EIP` by 4.
* Added segment memory load count, last address, selector, offset, and value to execution attempts and loader logs.
* Updated the `piu_1st` verification criteria in `scripts/test_all.ps1` to the new observation point.
* Recorded the current handling scope and next stop in `ARCHITECTURE.md` and `docs/TODO.md`.

## Verification

`powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`

Result: success.

`dos4gw_hello`:

* `Hello, world!` output is preserved.
* process exit code 0.

`piu_1st`:

* Passed the previous `26 8A 4F FF` stop.
* handled segment memory load count: 1
* last segment memory load address: `0x020F4D7D`
* last segment memory load register: `ES`
* last segment memory load selector: `0x0024`
* last segment memory load offset: `0x00000080`
* last segment memory load value: `0x00`
* next stop: `0x020F4DAC`
* next byte-window focus: `[8B] 06`
* previous segment load: `DS=0x002C`, source `0x021A664D`

As before, the external `spdlog` header may emit MSVC C4819 warnings, but the build succeeds.

## Next Work

The next stop is `8B 06`.
At the exception point, `DS=0x002C` and `ESI=0x00000000`, so the next policy to clarify is a DS-based low-memory or descriptor-based 32-bit memory read.
