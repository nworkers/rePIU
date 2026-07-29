# 20260730-363 Glide 호출 증가 성능 개선 계획 작업 로그 / Work log

* 설계: [20260730-363-glide-call-performance-plan.md](../design/20260730-363-glide-call-performance-plan.md)
* 작업 지시: [20260730-363-glide-call-performance-plan.md](../work-orders/20260730-363-glide-call-performance-plan.md)
* 기준 로그: `repiu_glide_profile_release_20260730_030212.txt` (로컬 산출물, Git 제외)

## 한국어

### 결과

최신 Release 실게임 profile을 재검토하고 다음 성능 작업을 재설계했습니다. 이번 작업은
문서만 변경했으며 성능 코드와 원본 실행 파일은 변경하지 않았습니다.

기준 실행은 약 47.5초 동안 Glide ordinal 403,904회와 `grBufferSwap` 1,287회를
완료했습니다. Glide gate는 wall-clock의 24.14%였습니다. 주요 상태 설정 18종은
303,399회, 프레임당 약 235.7회이며 wall의 20.59%, Glide의 85.33%를 차지했습니다.

세부 순위는 `grDepthMask` wall 8.30%, `grDrawTriangle` 2.71%,
`grAlphaBlendFunction` 2.24%입니다. `grDepthMask` backend의 94.3%는 host work,
`grDrawTriangle` backend의 94.1%는 queue/wake/complete handoff였습니다.

같은 실행에서 `grBufferSwap`은 wall 0.24%, SDL present는 0.17%였고 LFB lock은
한 번도 호출되지 않았습니다. texture download 63회도 wall 약 0.07%입니다.
따라서 Task 355의 LFB-first 우선순위를 이번 장면에 적용하지 않고 다음 순서로
대체했습니다.

1. Task 364: setter 반복률과 `grDepthMask`/alpha-blend 내부 phase 귀속
2. Task 365: 성공한 동일 상태만 보수적으로 host rendezvous 전에 생략
3. Task 366: 상태 최적화 뒤에도 필요할 때만 순서 보존 triangle batching
4. Task 367: 동일 바이너리 Release 3회 A/B와 전체 실행 축 재귀속

`current-execution-frontier.md`의 최상위 결론, Task 363 기록, 누적 표와 다음 순서를
이 기준으로 갱신했습니다. LFB 분해는 LFB가 실제 호출되는 별도 장면에서만 재개하며,
swap portability도 다른 host에서 interval 불일치나 cadence 결함이 확인될 때만
진행합니다.

### 검증

* `git diff --check`: 성공
* 변경 문서 UTF-8 strict decode: 성공
* 설계/작업 지시/frontier 상호 링크와 Task 364~367 순서 확인: 성공
* 변경 범위: 문서 4개
* 코드 변경 및 빌드: 없음
* `VERSION`: `0.0.113` 유지
* tag: 사용자 요청에 따라 생성하지 않음

---

## English

### Result

Re-reviewed the latest Release gameplay profile and replaced the current
performance order without changing runtime code or the original executable.

The run completed 403,904 Glide ordinals and 1,287 swaps in about 47.5
seconds. Glide occupied 24.14% of wall time. Eighteen major state setters
issued 303,399 calls, about 235.7 per frame, and held 20.59% of wall time or
85.33% of Glide.

`grDepthMask` held 8.30% of wall time, `grDrawTriangle` 2.71%, and
`grAlphaBlendFunction` 2.24%. Host work accounted for 94.3% of the depth-mask
backend interval, while queue/wake/complete handoff accounted for 94.1% of
the triangle backend interval.

The same run spent only 0.24% in `grBufferSwap` and 0.17% in SDL present,
made no LFB-lock calls, and spent about 0.07% on 63 texture downloads. The
older LFB-first order is therefore replaced by Task 364 setter/phase
attribution, Task 365 exact successful-state elision, Task 366 ordered
triangle batching only if still justified, and Task 367 whole-axis
re-attribution.

The current execution frontier now records the new top-level conclusion,
Task 363 evidence, cumulative result, and resumable order. LFB decomposition
is scene-conditional, and swap portability remains conditional on a
reproduced interval mismatch or cadence defect.

Validation passed `git diff --check`, strict UTF-8 decoding, link/order
inspection, and a documentation-only scope review. `VERSION` remains
`0.0.113`, and no tag is created per the explicit merge request.
