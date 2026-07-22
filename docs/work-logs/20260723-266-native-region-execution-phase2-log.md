# 20260723-266 작업 로그 (Phase 2): 네이티브 리전 실행기 — 하드웨어 브레이크포인트 방식, 메커니즘 실증 / Work log (Phase 2): native region executor — hardware-breakpoint mechanism, empirically proven

작업 지시: [docs/work-orders/20260723-266-native-region-execution.md](../work-orders/20260723-266-native-region-execution.md)
설계: [docs/design/20260723-266-native-region-execution.md](../design/20260723-266-native-region-execution.md)

## 한 일 / What was done

기존 `native_fast_path`(clean 함수 전체를 네이티브 실행)를 **민감 명령이 있는 함수도
네이티브 실행**하도록 확장. `REPIU_NATIVE_REGION` env 뒤에 두어 기본 동작 불변.

1. **리전 스캐너** `ScanNativeRegionWithZydis` (verified_region_analyzer): 함수를 거부하는
   대신 도달 가능한 HLE-민감 명령 주소를 수집. 직접 분기·직접 호출만 따라가고,
   간접/far 전송이 도달 가능하면 거부(민감 집합을 증명 불가하므로).
2. **공용 HLE 디스패치** `DispatchGuestHleHandlers`: single-step 경로의 HLE 핸들러 체인을
   추출(TF 무변경). single-step 경로와 리전 경로가 공유.
3. **리전 실행기** (하드웨어 브레이크포인트 방식):
   - `call rel32` 타깃 진입 시 스캔 → 민감 ≤3개면 Dr1-3에 실행 브레이크포인트, Dr0에 반환
     주소, TF 끄고 네이티브 실행. **게스트 바이트 미수정.**
   - 민감 명령 트랩(Dr1-3, 실행 전 fault) → `DispatchGuestHleHandlers`로 emulate·EIP 전진
     → 네이티브 유지. 반환(Dr0) → 리전 종료·single-step 복귀.
   - 민감 >3개 리전은 거부(single-step 유지).

## 실증 / Empirical result

**정합성·안전성: 확인됨.** Route A ON 30초·120초 완주, **크래시·조기 종료·fatal 없음**.
메커니즘(민감 명령 브레이크포인트 가로채기 + HLE + 네이티브 재개)이 정확·안전함을 입증.

**속도: 소폭 개선에 그침(정직한 결과).** 120초 A/B:

| | progress/120s | single_step | 비고 |
|---|---:|---:|---|
| OFF (기본) | 1,613,942 | 11,663,575 | clean fast path 28,258 |
| ON (Route A) | 1,655,673 | 11,440,622 | region 29,811진입/49,358히트/144거부/0 stray |

**speedup = +2.6%** (single_step −1.9%). 측정 상한 ~53x 대비 미미. 두 실행 모두 크래시·조기
종료·caught exception 0.

**근인(커버리지 한계):** (1) 진입이 `call rel32` 타깃에만, (2) HW-BP는 민감 ≤3개 제한
→ 세그먼트-집약 핫 함수(민감 ≥4)는 거부되어 single-step, (3) 리전이 짧아(평균 ~12명령)
진입/이탈 예외 비용이 이득을 상쇄. 대부분의 single-step을 차지하는 핫 루프가 리전에 진입
하지 못함.

## INT3(무제한 브레이크포인트) 시도와 철회 / INT3 attempt, reverted

먼저 INT3 코드 패치(무제한 민감 명령)로 구현했으나, 게스트 코드를 수정하는 방식이라
사전 존재 브레이크포인트/경계 desync로 조기 fatal(EXCEPTION_BREAKPOINT, 우리 패치가 아닌
주소)이 발생. self-heal 안전망으로도 미해결. 코드 미수정인 HW-BP 방식으로 전환해 안전성을
확보(HW-BP는 크래시 없음이 대조 실증). 무제한 커버리지를 위한 INT3의 견고화는 남은 과제.

## 다음 (전 10x를 위한 남은 작업) / Next (remaining for full 10x)

측정된 상한은 ~53x(민감 1.9%)지만, 실현하려면 커버리지를 넓혀야 한다:
1. **무제한 민감 브레이크포인트(INT3 견고화)** — HW-BP 4개 한도 제거. SMC/공유 콜리·경계
   desync·사전 INT3 구분을 안전하게 처리.
2. **넓은 진입** — `call rel32` 타깃뿐 아니라 핫 루프 진입점에서도 리전 시작(반환 경계
   부재 문제 해결 필요).
3. **간접 분기 리전 이탈** — 간접 분기를 리전 경계로 처리해 리전을 길게.

## 변경 파일 / Files

- `src/platform/win32/verified_region_analyzer.{h,cpp}` (`ScanNativeRegionWithZydis`)
- `src/platform/win32/native_fast_path.h` (리전 상태 필드)
- `src/platform/win32/execution/execution_trampoline.cpp` (`DispatchGuestHleHandlers`,
  `TryEnterNativeRegion`, `HandleNativeRegionSensitiveDr`, `LeaveNativeRegion`, 핸들러 통합)
- `src/platform/win32/telemetry/live_telemetry_snapshot.cpp` (`region=` 노출)

## 검증 절차 / Verification

`REPIU_NATIVE_REGION` on/off 120초 A/B, supervisor. 무회귀(조기 종료·fatal 없음) 확인.
