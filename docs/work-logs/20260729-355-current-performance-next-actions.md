# 20260729-355 현재 성능 다음 작업 기록 작업 로그 / Work log

* 설계: [20260729-355-current-performance-next-actions.md](../design/20260729-355-current-performance-next-actions.md)
* 작업 지시: [20260729-355-current-performance-next-actions.md](../work-orders/20260729-355-current-performance-next-actions.md)

## 한국어

### 결과

Task 354의 최신 동일 실행 산출물을 재집계해 다음 성능 작업을 확정했습니다.

1. Task 356: `grLfbLock` phase와 guest overwrite coverage
2. Task 357: frame-local Glide command handoff census
3. Task 358: 편향 없는 guest 실행 residency 귀속

최신 control 중앙값은 guest 실행 54.32%, Glide 30.63%, VEH-exclusive 7.49%,
커널 전이 6.98%입니다. matching profile에서 `grLfbLock`은 wall 약 5.19%,
handoff 지배 API 17개는 wall 약 12.10%입니다. 이 수치는 Task 354 profile의 Glide
중앙값을 사용했으며 Task 353의 다른 실행 비중과 혼합하지 않았습니다.

`grLfbLock`은 WRITE_ONLY에서도 전체 readback을 수행하지만 guest가 부분 쓰기할 때
원본 framebuffer 보존이 필요하므로 overwrite coverage 확인 전에는 제거하지 않습니다.
handoff 호출도 값과 순서를 관측하기 전에 합치지 않습니다. guest residency는 기각된
sampling 방식을 재사용하지 않습니다.

코드 변경은 없으며 문서와 patch version만 갱신했습니다.

---

## English

### Result

Re-aggregated Task 354's latest matched artifacts and fixed the next order as
Task 356 `grLfbLock` phase/overwrite coverage, Task 357 frame-local Glide
handoff census, and Task 358 unbiased guest residency attribution.

The latest control medians are 54.32% guest execution, 30.63% Glide, 7.49%
VEH-exclusive, and 6.98% kernel transitions. The matching profile places
`grLfbLock` at about 5.19% of wall time and seventeen handoff-dominated APIs at
about 12.10%. No readback or command is removed before its semantic
preconditions are proven, and rejected biased sampling is not reused. This
task changes documentation and the patch version only.
