# 20260801-387 Glide gate 전용 DBT dispatch 작업 로그

## 한국어

Task 386 Music Select 캡처에서 합성 Glide gate가 AOT boundary의 88.33%를 차지한 결과를 바탕으로, Win32 `aot-dbt` 전용 opt-in 직접 경로를 구현했습니다.

구현 결과:

- 자산 유래 gate address, ordinal, argument ABI와 기존 `UD2 + ordinal + RET` 바이트를 검증합니다.
- 검증된 8바이트 gate를 `CALL rel32 + RET imm16`으로 바꾸며, RET immediate에는 argument bytes만 기록합니다.
- 전용 naked thunk가 guest register/EFLAGS를 보존하고 host stack에서 기존 `HandleGlideGateBoundary`를 호출합니다.
- 첫 AOT cache boundary에서 같은 gate boundary를 가리키는 direct fixup과 indirect inline-cache target을 executable gate로 재연결합니다.
- 이후 transfer resolution은 일반 excluded-range 거부 전에 검증된 Glide gate를 직접 반환합니다.
- opt-in gate page만 `PAGE_EXECUTE_READ`로 바꾸고 instruction cache를 flush합니다.
- 기본값은 OFF이며 `REPIU_AOT_DBT_GLIDE_GATE_DISPATCH=1|on|true`로 활성화합니다.

검증 결과:

- Release loader와 AOT probe 빌드 성공.
- 전체 AOT probe: `glide_direct_dispatch_layout=true`, `selector_guard_all=true`, `coherence_all=true`.
- 1초 ABI 수정 스모크: direct `49/49/0/0`, 최종 예외 없음.
- 5초 opt-in: patched/verified/resolved/relinked `172/172/115/389`, direct entry/success/target-miss/terminal `65,241/65,240/0/0`, 최종 예외 없음.
- 5초 opt-out 대비 총 예외 `140,313 -> 51,601`(-63.22%), breakpoint `73,340 -> 27,792`(-62.10%), single-step `53,792 -> 8,771`(-83.70%), AOT other boundary `71,776 -> 26,754`(-62.72%).

짧은 스모크의 Glide 처리량 증가는 성능 이득을 지지하지만 장면 진행 차이가 있을 수 있으므로, 기본값 승격 전 Music Select 수동 캡처가 남아 있습니다.

## English

Based on the Task 386 Music Select capture showing synthetic Glide gates at 88.33% of AOT boundaries, this task implemented an opt-in Win32 `aot-dbt` direct path.

Implementation:

- Validate asset-derived gate address, ordinal, argument ABI, and original `UD2 + ordinal + RET` bytes.
- Rewrite each validated eight-byte gate as `CALL rel32 + RET imm16`, with argument bytes only in the RET immediate.
- Use a dedicated naked thunk to preserve guest registers/EFLAGS and call the existing `HandleGlideGateBoundary` on the host stack.
- At the first AOT cache boundary, relink matching direct fixups and indirect inline-cache targets to the executable gate.
- Return validated Glide gates before general excluded-range rejection in later transfer resolution.
- Make only the opt-in gate page `PAGE_EXECUTE_READ` and flush its instruction cache.
- Keep the default OFF; enable with `REPIU_AOT_DBT_GLIDE_GATE_DISPATCH=1|on|true`.

Verification:

- Release loader and AOT probe builds passed.
- Full AOT probe passed with `glide_direct_dispatch_layout=true`, `selector_guard_all=true`, and `coherence_all=true`.
- One-second ABI-fixed smoke: direct `49/49/0/0`, no final exception.
- Five-second opt-in: patched/verified/resolved/relinked `172/172/115/389`, direct entry/success/target-miss/terminal `65,241/65,240/0/0`, no final exception.
- Versus the five-second opt-out, total exceptions fell `140,313 -> 51,601` (-63.22%), breakpoints `73,340 -> 27,792` (-62.10%), single steps `53,792 -> 8,771` (-83.70%), and AOT other boundaries `71,776 -> 26,754` (-62.72%).

The short smoke supports a throughput gain, but scene progress can differ; a manual Music Select capture remains required before promoting the default.
