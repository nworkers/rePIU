# FS segment word memory load HLE 작업 로그

## 결과

`piu_1st`가 `66 65 8B /r` FS segment word memory load 지점에서 중단되지 않도록 제한적인 HLE를 추가했다.

* `WriteRegister16` helper를 추가해 ModRM reg 필드가 가리키는 16-bit general register 하위 word를 갱신한다.
* `ReadFsSegmentWord` helper를 추가해 현재 guest FS selector와 일치하고 offset이 `0x10000` 미만인 word read를 `0`으로 반환한다.
* `HandleFsSegmentWordLoadInstruction`을 추가해 관찰된 `0x66 0x65 0x8B` 형식 중 SIB 없는 base register addressing과 8-bit displacement 형식을 처리한다.
* 처리 결과를 기존 segment memory load 진단 로그에 기록한다.
* `scripts/test_all.ps1`의 `piu_1st` 기대 지점을 새 블로커에 맞춰 갱신했다.

## 관찰

이 변경 후 `piu_1st`는 relocated base + `0x000F246F`의 FS word load 지점을 통과하고, relocated base + `0x000F2098`의 `INT3` opcode `0xCC`에서 중단된다.

마지막으로 기록된 segment memory load는 다음과 같다.

* address: relocated base + `0x000F246F`
* opcode: `0x8B`
* segment register: `FS`
* selector: `0x002C`
* offset: `0x00000042`
* width: `2`
* value: `0x00`

현재 구현은 완전한 FS descriptor 또는 FS-backed memory 모델이 아니다. 관찰된 low-offset FS word read를 안전하게 zero-fill해 다음 요구사항을 드러내기 위한 임시 HLE이다.

## 검증

다음 명령으로 검증했다.

* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1 -SkipSetup`

결과는 통과했다. Win32 x86 빌드 중 third-party `spdlog` header의 코드 페이지 경고 `C4819`가 계속 표시되지만 이번 변경과 직접 관련된 실패는 아니다.

# FS Segment Word Memory Load HLE Work Log

## Result

Added limited HLE so `piu_1st` no longer stops at the `66 65 8B /r` FS segment word memory load point.

* Added a `WriteRegister16` helper to update the low word of the 16-bit general register selected by the ModRM reg field.
* Added a `ReadFsSegmentWord` helper that returns `0` for word reads where the selector matches the current guest FS selector and the offset is below `0x10000`.
* Added `HandleFsSegmentWordLoadInstruction` for the observed `0x66 0x65 0x8B` form with non-SIB base register addressing and optional 8-bit displacement.
* Recorded handled reads through the existing segment memory load diagnostics.
* Updated the `piu_1st` expectation in `scripts/test_all.ps1` to the new blocker.

## Observation

After this change, `piu_1st` passes the FS word load point at relocated base + `0x000F246F` and stops at the `INT3` opcode `0xCC` at relocated base + `0x000F2098`.

The last recorded segment memory load is:

* address: relocated base + `0x000F246F`
* opcode: `0x8B`
* segment register: `FS`
* selector: `0x002C`
* offset: `0x00000042`
* width: `2`
* value: `0x00`

This is not a complete FS descriptor or FS-backed memory model. It is a temporary forward-progress HLE that safely zero-fills the observed low-offset FS word read and reveals the next requirement.

## Verification

Verified with:

* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1 -SkipSetup`

The verification passed. The Win32 x86 build still reports the existing third-party `spdlog` code-page warning `C4819`, but it is not a failure caused by this change.
