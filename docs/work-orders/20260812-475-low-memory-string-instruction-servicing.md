# Low memory string instruction 대행 작업 지시

1. `SetCompareFlags8`을 폭 인자를 받는 `SetCompareFlags`로 일반화하고 기존 함수는 이를
   호출하도록 바꿉니다. 8-bit 결과는 그대로여야 합니다.
2. `low_memory_string_access` 전용 header와 source를 추가하고 `SCAS`, `LODS`, `CMPS`의
   1회 반복 대행과 내부 반복, 종료 판정, `EIP` 전진 여부를 그 안에 둡니다.
3. address size 32-bit가 아니거나 접근 폭이 low memory 경계를 걸치면 거부합니다. `CMPS`
   반대편은 guest 범위 검사를 통과할 때만 읽습니다.
4. `HandleGuestLowMemoryReadFault`에 이 모듈을 먼저 시도하는 adapter 호출만 추가합니다.
   실패 시 기존 `MOV` 경로가 그대로 동작해야 합니다.
5. thread context에 대행 계수를 두고 telemetry snapshot과 loader 진단으로 전달합니다.
6. Win32 x86 Debug 빌드를 수행합니다.
7. `pumpit8`을 실행해 `+0xE5D0D` 접근 위반 소멸과 실행 진행을 확인합니다.
8. 분석 문서와 작업 로그를 갱신하고 하나의 작업 커밋으로 남깁니다.

# Low Memory String Instruction Servicing Work Order

1. Generalize `SetCompareFlags8` into a width-taking `SetCompareFlags` and make the original
   delegate to it, leaving 8-bit results unchanged.
2. Add a dedicated `low_memory_string_access` header and source holding one-iteration servicing for
   `SCAS`, `LODS`, and `CMPS`, plus internal iteration, termination testing, and the `EIP` advance
   decision.
3. Decline when the address size is not 32-bit or the access straddles the low-memory boundary.
   Read the other side of a `CMPS` only when the guest-range check accepts it.
4. Add only the adapter call trying this module first in `HandleGuestLowMemoryReadFault`, leaving
   the existing `MOV` path intact on failure.
5. Keep servicing counters in the thread context and carry them through the telemetry snapshot and
   loader diagnostics.
6. Run the Win32 x86 Debug build.
7. Run `pumpit8` and confirm the `+0xE5D0D` access violation is gone and execution proceeds.
8. Update the analysis document and work log, and leave one task commit.
