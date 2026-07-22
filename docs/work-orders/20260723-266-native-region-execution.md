# 20260723-266 작업 지시: 네이티브 리전 실행 / Work order: native region execution

설계: [docs/design/20260723-266-native-region-execution.md](../design/20260723-266-native-region-execution.md)

## 목표 / Goal

single-step-everything 모델을 걷어내고, HLE-민감 명령(동적 1.9%)에만 트랩을 걸어 나머지
98.1%를 네이티브로 실행함으로써 실행 속도를 10배 이상(상한 ~53배) 끌어올린다.

Remove the single-step-everything model; trap only HLE-sensitive instructions (1.9% of the
dynamic stream) and run the other 98.1% natively, for a 10x+ speedup (ceiling ~53x).

## 범위 / Scope

- Phase 0 (완료): 계측·baseline·LDT 스파이크.
  - `thread_context.h`: `routea_sensitive_count`, `routea_segment_sensitive_count`.
  - `execution_trampoline.cpp`: `ClassifyRouteASensitive` + `HandleSingleStepTrace` 계측.
  - `live_telemetry_snapshot.cpp`: `routea=%u/%u` 노출.
- Phase 1: 리전 스캐너 — `verified_region_analyzer`에 민감 명령 주소 집합 반환 모드.
- Phase 2: 리전 실행기 — BP 설치(HW BP 우선, INT3 확장)·트랩→기존 shadow HLE→네이티브 재개.
- Phase 3: INT3 사용 시 SMC 일관성(`aot_page_write_watch` 재사용).
- Phase 4: 진입 확대(call rel32 외 일반 리전).
- Phase 5: 이탈/폴백/체이닝 정리.

## 착수 순서 / Order

1. (완료) Phase 0. baseline: legacy 1.83M/120s, 민감 1.88%(217,529/11,581,526), 상한 ~53x.
2. Phase 1 → Phase 2를 최소 기능으로 붙여 A/B로 speedup 실증(먼저 HW BP 4개 한도 리전만).
3. 실증되면 INT3 확장으로 리전 크기 제한 해제.

## 검증 / Verification

- 통제 A/B(REPIU 환경변수로 Route A on/off), 120초 동일 시점.
- 합격: progress 대폭 증가(목표 ≥10x), single_step 대폭 감소, `routea` 트랩만 남음,
  마지막 EIP·Glide swap·`fatal_count=0` 무회귀.

## 리스크 / Risks

- HW BP 4개 제한 → 고밀도 리전은 INT3 필요(Phase 3 선결).
- non-override default 접근의 세그먼트 정확성(§2, 위험 낮음, Phase 2 불변식 재확인).
- 백엔드 정확성 동등성(렌더링은 aot-dynamic에서 검증됨).
