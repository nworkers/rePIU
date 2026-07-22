# AOT 이탈 심층 계측: `other` 사유 규명(a)과 체류량(b)
# Design: Deep AOT-Exit Instrumentation — Characterize `other` (a) and Residency (b)

## 1. 배경 (Background)

Task 262 실측은 `aot-dynamic` 경계 이탈 73,326건 중 **`other`(비전달 명령)가
77.6%**, `indirect` 12.7%, `return` 9.8%, `direct`·`conditional` 0임을 확인했다.
간접 분기 인라인 캐시 churn 가설은 주요 비용으로는 반증됐고, 남은 질문은 두 가지다.

* **(a)** `other` 77.6%는 **정확히 어떤 명령/위치**인가? 미지원 명령 stop인지, SMC
  재-sentinel화인지, 특정 핫 EIP에 몰렸는지.
* **(b)** AOT가 실제로 **얼마나 커버**하는가? 이탈 카운트만으로는 블록 진입당 네이티브
  실행량(체류량)을 알 수 없다. 커버리지 분모가 없다.

Task 262 confirmed the boundary distribution; the open questions are (a) what the
77.6% `other` bucket actually is (which instruction/EIP), and (b) how much AOT
actually covers — a residency denominator that exit counts alone cannot give.

## 2. (a) `other` 경계 특성화 (Characterizing the `other` bucket)

`BumpAotBoundaryReason` 호출 지점에서 사유가 `kOther`일 때 표본을 기록한다.

1. **선두 opcode 히스토그램.** `ThreadContext`에 `aot_other_opcode_histogram[256]`
   (게스트 스레드 전용, 비원자). 매 `other` 경계에서 `histogram[bytes[0]]++`. 어떤
   명령 계열이 지배하는지 O(1)로 집계.
2. **핫 EIP 표본.** 가장 최근 `other` 경계의 게스트 EIP와 선두 4바이트를
   `aot_last_other_boundary_eip`/`aot_last_other_boundary_bytes`에 기록.
3. **노출.** 공유 텔레메트리에 top-1 opcode(running max, O(1))·top count·last
   eip·last bytes를 미러링(supervisor가 종료와 무관하게 관측). 정상 종료 요약에는
   히스토그램 top-8 opcode:count를 출력.

이 계측으로 supervisor 주기 덤프에서 top opcode와 최근 EIP를, 요약에서 top-8 분포를
읽어 77.6%의 정체를 규명한다. 필요하면 EIP를 `repiu_aot_probe --dump/--xref`로
역어셈블한다.

## 3. (b) AOT 체류량 프록시 (Residency proxy)

실제 캐시 진입 시점에서, **게스트 진입 EIP부터 첫 제어 전달 명령까지 직선(straight-
line) 명령 수**를 Zydis로 디코드해 세어 누적한다. 이 값은 그 블록이 이탈 전 네이티브로
실행하는 명령 수의 하한 근사다(직선 basic-block 길이).

* 훅 위치: 명확한 게스트 target이 있는 캐시 진입 — `HandleAotIndirectTransfer`,
  `HandleAotReturnTransfer`, `HandleAotReentry`의 retired 재해결 진입.
* 누적: `aot_residency_instruction_total`, `aot_residency_sample_count`,
  `aot_residency_max`. 디코드는 첫 전달 명령(ret/call/jmp/jcc/indirect) 또는 상한
  64명령 또는 판독 불가에서 멈춘다(무한 루프 방지).
* 커버리지 추정: `total / (total + single_step_trace_count)`. 이탈 카운트가 아닌
  **네이티브 실행 명령 대 단일스텝 명령** 비율을 준다.
* 노출: 공유 텔레메트리에 total/samples/max 미러, supervisor·정상 종료 요약에 평균과
  커버리지 추정 출력.

**한계 (명시).** 직선 길이는 direct branch 체이닝으로 이어지는 네이티브 실행을
따라가지 않으므로 체류량을 **과소평가**한다. 그럼에도 "블록이 이탈 전 얼마나 짧은가"의
1차 지표로 충분하며, 단일스텝 지배(98.6%)와 교차검증된다.

## 4. 계측 표면 (Instrumentation surface)

* `thread_context.h`: (a) 히스토그램·last-other 필드, (b) residency 누적 3개.
* `aot_runtime_dispatch.{h,cpp}`: `RecordAotOtherBoundarySample`,
  `AccumulateAotResidency` 추가 및 호출.
* `live_telemetry.h`: (a) 4필드 + (b) 3필드, `kWin32LiveTelemetryVersion` 17 → 18.
* `execution_trampoline.h` / `live_telemetry_snapshot.cpp`: 요약 필드(히스토그램
  top-8 포함)·복사.
* `host/win32/main.cpp`, `supervisor_main.cpp`: 출력 한두 줄.

## 5. 영향 범위 (Impact Scope)

순수 진단 계측. 게스트 실행 동작 불변(분기·레지스터 조작 없음, 디코드·카운트·텔레메트리
쓰기만). 디코드는 판독성 검사 후 수행. 공유 구조체 레이아웃 변경으로 버전 상수를 함께
올린다.

**English.** (a) On each `other` boundary, bump a 256-entry lead-opcode histogram
and record the last `other` EIP + 4 bytes; mirror the top opcode and last sample
to shared telemetry and print the top-8 in the graceful summary — enough to
identify what the 77.6% is (then disassemble the EIP with `repiu_aot_probe`). (b)
At real cache entries (indirect/return/retired-reresolve), decode-count
straight-line guest instructions from the entry to the first control transfer
(cap 64), accumulate total/samples/max, and report average residency plus a
coverage estimate `total/(total+single_step)`. Pure diagnostics; guest execution
unchanged; telemetry version bumped 17 → 18. The straight-line proxy undercounts
residency across chained direct branches and is labeled a lower bound.
