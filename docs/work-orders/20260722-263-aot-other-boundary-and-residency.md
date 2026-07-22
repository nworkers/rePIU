# 작업 지시: AOT `other` 경계 특성화(a) + 체류량(b)
# Work Order: Characterize `other` boundaries (a) + Residency (b)

**설계 (Design):** `docs/design/20260722-263-aot-other-boundary-and-residency.md`
**Task:** 263 (Task 262 후속 a·b)

## 변경 항목 (Change list)

1. `thread_context.h` — (a) `aot_other_opcode_histogram[256]`,
   `aot_last_other_boundary_eip`, `aot_last_other_boundary_bytes`; (b)
   `aot_residency_instruction_total`, `aot_residency_sample_count`,
   `aot_residency_max`.
2. `aot_runtime_dispatch.{h,cpp}` — `RecordAotOtherBoundarySample`(a),
   `AccumulateAotResidency`(b) 추가 및 호출(kOther 경계, 캐시 진입 3지점).
3. `live_telemetry.h` — (a) `aot_other_top_opcode`,
   `aot_other_top_opcode_count`, `aot_last_other_eip`, `aot_last_other_bytes`;
   (b) `aot_residency_total`, `aot_residency_samples`, `aot_residency_max`;
   `kWin32LiveTelemetryVersion` 17 → 18.
4. `execution_trampoline.h` — 요약 필드(히스토그램 top-8 opcode/count, last-other,
   residency).
5. `live_telemetry_snapshot.cpp` — top-8 계산·복사.
6. `host/win32/main.cpp` — top-8 opcode 분포 + residency/coverage 요약.
7. `supervisor_main.cpp` — top opcode·last-other·residency 주기 출력.

## 검증 (Verification)

* VS 2026 Debug 빌드.
* supervisor로 `pumpit1` 120초 aot-dynamic 1회 구동 → `other` top opcode/EIP 분포와
  residency/coverage 수치 확인. 필요 시 top EIP를 `repiu_aot_probe`로 역어셈블.

## 완료 조건 (Done)

* (a) `other` 77.6%를 지배하는 opcode 계열과 대표 EIP 식별.
* (b) 평균 체류량(블록 진입당 직선 명령 수)과 커버리지 추정치 산출.
* 설계·지시·로그·frontier 갱신.
