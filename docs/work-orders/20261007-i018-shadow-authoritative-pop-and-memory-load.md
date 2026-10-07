# 작업 지시: shadow 진실원 pop 슬롯과 메모리-소스 로드 슬롯 (issue #18, 방향 3 2단계)

설계: `docs/design/20261007-i018-shadow-authoritative-pop-and-memory-load.md`

## 작업 항목

1. 사이트 구조 (`include/repiu/runtime/aot_code_cache.h`)
   * `AotGuardedSegmentPopSite`에 `pair0_address_offset`,
     `pair1_address_offset`, `shadow_store_offset` 추가(0 = 옛 레이아웃).
2. 분류기 (`src/runtime/aot_translation_plan.cpp`, 헤더에 상수)
   * `kAotSegmentLoadMemorySource` 상수. `ReadGuardedSegmentLoadRegisters`가
     설계의 조건으로 메모리 소스를 받아 `gpr_register`에 상수를 쓴다.
3. 이미터·검증기 (`src/runtime/aot_code_cache.cpp`)
   * `EmitGuardedSegmentPopSlot`을 설계의 66바이트 슬롯으로 교체.
   * 소스 피연산자 부호화 헬퍼(레지스터/[esp]/메모리, ESP 베이스 +8
     재부호화). `EmitGuardedSegmentLoadSlot`이 헬퍼를 쓰고 메모리 소스를
     받는다.
   * `ValidateAotCodeCacheHleCoverage`의 i386 pop·load 단언을 새
     레이아웃(가변 소스 길이)으로 갱신. `estimated_emitted_bytes` 조정.
4. 패처 (`src/runtime/aot_segment_patch.cpp`)
   * `PatchAotGuardedSegmentPopSites`가 새 필드를 로드 패처와 같은
     규칙으로 패치.
5. 엔진
   * 동적 추가의 pop 사이트 오프셋 보정(`src/engine/aot_code_cache.cpp`).
   * guard-fault 창 끝을 pop 사이트의 `shadow_store_offset`까지
     (`src/engine/aot/aot_guard_compare_fault.cpp`).
6. probe (`src/tools/aot_probe/selector_guard_probe.cpp`)
   * pop 슬롯 레이아웃·커버리지 단언 갱신.
   * 메모리-소스 로드: `66 2E 8E 1D disp32` 분류·슬롯 바이트, `8E 44 24 08`
     의 ESP 재부호화(`66 8B 84 24 10 00 00 00`), `67` 접두어 거부, 검증기
     통과.
7. 검증 (설계의 절차) 및 분석 문서 갱신
   * `--selector-guard`·`--segment-restore` 통과, pumpitea 교대 측정,
     스모크. 분석 문서의 "mov es,[saved]" 추정을 `pop es`로 교정하고
     ISR 말미 pop 네 개·진입 헬퍼 부호화·`0xFCF6F` 관용구를 확정 사실로
     기록.

## 완료 기준

* pumpitea 로딩 중 memcpy 복원 pop과 ISR 진입 로드·말미 pop ds가 INT3
  없이 돌고, guarded pop 폴백과 handled segment load가 틱·memcpy 수에
  비례하지 않는다.
* 공백이 통제 프로토콜에서 1단계 기준선(6.8~7.6초)보다 짧거나 같다.

---

# Work order: shadow-authoritative pop and memory-source load slots (issue #18, direction 3 phase 2)

Design: `docs/design/20261007-i018-shadow-authoritative-pop-and-memory-load.md`

Items: (1) add `pair0_address_offset`, `pair1_address_offset` and
`shadow_store_offset` to `AotGuardedSegmentPopSite` (zero = old
layout); (2) `kAotSegmentLoadMemorySource` and memory-source acceptance
in `ReadGuardedSegmentLoadRegisters` under the design's conditions;
(3) the 66-byte i386 pop slot, a shared source-operand encoder
(register, `[esp]`, memory with the ESP-base +8 rewrite) used by the
load emitter and the coverage validator, updated i386 pop/load
assertions and byte estimates; (4) pop patcher support for the new
fields on the load patcher's rule; (5) engine: dynamic-append offset
fixes for pop sites and the guard-fault window end; (6) probe: updated
pop layout/coverage assertions, memory-source cases (`66 2E 8E 1D
disp32`, the ESP re-encoding of `8E 44 24 08` to `66 8B 84 24 10 00 00
00`, `67` rejection, validator pass); (7) verification per the design
and the analysis-topic corrections (`pop es`, the four epilogue pops,
the helper's encoding, the `0xFCF6F` idiom).

Done when the memcpy restore pop, the ISR entry load and the epilogue
`pop ds` run without INT3 during pumpitea loading, the pop fallback and
handled-load counts stop scaling with ticks and memcpys, and the gap is
no worse than the phase-1 baseline under the controlled protocol.
