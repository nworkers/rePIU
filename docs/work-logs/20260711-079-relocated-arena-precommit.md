# relocated arena 선점 예약 작업 로그

## 변경 내용

`piu_1st`가 relocated image placement 전 `VirtualAlloc` error `487`로 중단되는 문제를 줄이기 위해, relocated base 후보 선택 기준을 실제 arena reserve/commit 성공으로 바꿨다.

기존에는 `VirtualQuery`로 후보 주소가 비어 있는지 확인한 뒤, 여러 준비 단계를 거쳐 나중에 `VirtualAlloc(MEM_RESERVE | MEM_COMMIT)`을 실행했다. 이제는 후보 주소가 `available`이면 즉시 `MEM_RESERVE | MEM_COMMIT`으로 선점 예약하고, 성공한 reservation의 base를 relocated image base로 사용한다.

추가한 구조는 다음과 같다.

* `ReserveAndCommitWin32RuntimeAddressRange`
* `PlaceWin32RelocatedImageInReservedRange`
* loader의 `SelectAndReserveRelocatedImageBase`

`PlaceWin32RelocatedImageInReservedRange`는 이미 확보된 arena에 object bytes를 복사하고 object protection을 적용한다. 따라서 placement 단계에서 같은 주소를 다시 `VirtualAlloc`하지 않는다.

`scripts/test_all.ps1`의 `piu_1st` 현재 관측 주소도 최신 실행 결과인 `0x020F4386`으로 갱신했다.

## 결과

`piu_1st`는 더 이상 relocated image placement 단계에서 `VirtualAlloc relocated image failed with error 487`로 중단되지 않는다.

순차 실행 기준 현재 관측 지점은 다시 다음 Port I/O blocker이다.

* exception code: `0xC0000096`
* exception address: `0x020F4386`
* last port I/O opcode: `0x66EF`
* last port I/O port: `0x02A0`
* last port I/O value: `0x00000005`
* last port I/O result: `unsupported`

## 검증

* `cmd /c scripts\build_win32_x86.bat`
  * 결과: 통과
  * 참고: 기존 spdlog code page warning `C4819`는 유지됨
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
  * 결과: 통과
  * 확인: precommitted placement 후 `0x02A0/0x5` Port I/O blocker 도달
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1 -SkipSetup`
  * 결과: 통과

sandbox 내부 빌드는 `build\win32_x86_debug\_deps\spdlog-subbuild\CMakeFiles\generate.stamp` timestamp 접근 거부로 실패했으므로, 빌드와 전체 테스트는 승인된 외부 권한으로 수행했다.

# Relocated Arena Precommit Work Log

## Changes

Changed relocated base candidate selection to use real arena reserve/commit success so `piu_1st` no longer stops before relocated image placement with `VirtualAlloc` error `487`.

Previously, the loader used `VirtualQuery` to observe that a candidate address range was free, performed several preparation steps, and later called `VirtualAlloc(MEM_RESERVE | MEM_COMMIT)`. Now, when a candidate is `available`, the loader immediately precommits it with `MEM_RESERVE | MEM_COMMIT` and uses the successful reservation base as the relocated image base.

Added:

* `ReserveAndCommitWin32RuntimeAddressRange`
* `PlaceWin32RelocatedImageInReservedRange`
* loader-side `SelectAndReserveRelocatedImageBase`

`PlaceWin32RelocatedImageInReservedRange` copies object bytes and applies object protection inside an already acquired arena, so placement no longer calls `VirtualAlloc` for the same address again.

Updated the current `piu_1st` observation address in `scripts/test_all.ps1` to the latest run result, `0x020F4386`.

## Result

`piu_1st` no longer stops at relocated image placement with `VirtualAlloc relocated image failed with error 487`.

The current sequential-run observation point is again the Port I/O blocker:

* exception code: `0xC0000096`
* exception address: `0x020F4386`
* last port I/O opcode: `0x66EF`
* last port I/O port: `0x02A0`
* last port I/O value: `0x00000005`
* last port I/O result: `unsupported`

## Verification

* `cmd /c scripts\build_win32_x86.bat`
  * Result: passed
  * Note: the existing spdlog code page warning `C4819` remains
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
  * Result: passed
  * Checked: reached the `0x02A0/0x5` Port I/O blocker after precommitted placement
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1 -SkipSetup`
  * Result: passed

The sandboxed build failed because access to `build\win32_x86_debug\_deps\spdlog-subbuild\CMakeFiles\generate.stamp` was denied while restoring the CMake timestamp, so the build and full test run were executed with approved external permissions.
