# 20260723-266 작업 로그 (Phase 0): 네이티브 리전 실행 — 진단·baseline·LDT 스파이크·민감도 계측 / Work log (Phase 0)

작업 지시: [docs/work-orders/20260723-266-native-region-execution.md](../work-orders/20260723-266-native-region-execution.md)
설계: [docs/design/20260723-266-native-region-execution.md](../design/20260723-266-native-region-execution.md)

## 한 일 / What was done

1. **근본 병목 확정.** legacy는 실행 시작 트램폴린(execution_trampoline.cpp:407-411)이 TF를
   켜 모든 게스트 명령을 single-step. 두 백엔드 모두 single-step이 dispatch의 99%+.
2. **v0.0.84 fresh baseline 실측**(Debug, supervisor, 120초): legacy progress 1,829,006 /
   single_step 12,677,443; aot-dynamic 127,073 → legacy 14.4x. AOT는 순손실 재확인.
3. **LDT 스파이크**(scratchpad/ldt_spike.cpp, 32비트): `NtSetLdtEntries`가 게스트 셀렉터
   전부(0x1C/1F/24/27/2C/2F/34/37)에 STATUS_NOT_IMPLEMENTED → Win11 x64 WOW64는 LDT 미지원.
   실제 디스크립터로 세그먼트를 네이티브화하는 길 배제.
4. **민감도 계측 추가·실측.** `HandleSingleStepTrace`에 `ClassifyRouteASensitive`(Zydis 1회
   디코드, verified_region_analyzer의 `IsSensitive` 미러) 추가, `routea_sensitive_count`/
   `routea_segment_sensitive_count` 카운터, [repiu-live]에 `routea=%u/%u` 노출.
   legacy 120초: 총 11,581,526 중 민감 217,529(**1.88%**), 세그먼트 164,499(민감의 75.6%).
   → **98.1% 네이티브화 가능, 상한 ~53배.**
5. 설계·작업지시·분석 프론티어(Task 266) 문서화.

## 변경 파일 / Files

- `src/platform/win32/execution/thread_context.h` (+2 카운터)
- `src/platform/win32/execution/execution_trampoline.cpp` (`ClassifyRouteASensitive` + 계측)
- `src/platform/win32/telemetry/live_telemetry_snapshot.cpp` (`routea=%u/%u`)
- `docs/design/20260723-266-*`, `docs/work-orders/20260723-266-*`,
  `docs/analysis/current-execution-frontier.md`

## 빌드·검증 / Build & verify

- `scripts/build_win32_x86.ps1` 성공(exit 0). 계측 런 정상 완료(child_exit 정상, fatal=0).
- 계측 오버헤드로 총 명령이 12.68M→11.58M(~9%↓)이나 **비율(1.88%)이 산출물**이라 무방.

## 다음 / Next

Phase 1(리전 스캐너) → Phase 2(리전 실행기, HW BP 최소 기능) A/B 실증.
