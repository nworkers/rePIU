# Single-step hotspot 검토 / Single-step hotspot review

Task 380. 근거: 2026-08-01 `single-step-hotspot-capture.log`.

## 확인 결과

hotspot profile은 85,713개의 trace-owned single-step을 기록했습니다. 결과별 횟수는 HLE/timer/native/TF가 `66,152/12/0/19,549`이고, 시간은 `3,641,953,045/741,924/0/57,878,284` cycle입니다. 즉 profile 시간의 약 98.43%는 HLE 처리 결과이고, TF 재설정 자체는 약 1.56%입니다.

post-HLE AOT 복귀는 66,138회 중 54,270회(82.06%) 성공했습니다. 11,868회(17.94%)의 span-safety 거절은 다음 후보이지만 전체 hotspot 비용의 주원인이라고 확정할 수 없습니다.

cycle 상위 지점 중 `0x030F3BAD`, `0x030F3BBD`, `0x030F5637`은 HLE dispatch 시간이 지배적이고, `0x030F536A`, `0x030F536C`, `0x0303391A` 등은 AOT-resume 시간이 큽니다. 다음 작업은 이 EIP들이 어떤 HLE 명령/ordinal과 이어지는지 식별하고, 원본 guest 동작을 보존하는 직접 처리 또는 AOT 복귀 확대 후보를 한 지점씩 검토합니다.

## English

The hotspot profile recorded 85,713 trace-owned single steps. Outcome counts HLE/timer/native/TF were `66,152/12/0/19,549`, and cycles were `3,641,953,045/741,924/0/57,878,284`. About 98.43% of profiled time belongs to HLE outcomes; TF re-arm itself is only about 1.56%.

Post-HLE AOT re-entry succeeded 54,270 times out of 66,138 (82.06%). The 11,868 span-safety rejections (17.94%) are a candidate for later work, but are not yet proven to dominate total hotspot cost.

Among cycle leaders, `0x030F3BAD`, `0x030F3BBD`, and `0x030F5637` are dominated by HLE-dispatch time, while `0x030F536A`, `0x030F536C`, and `0x0303391A` have large AOT-resume time. The next task should identify the HLE instruction/ordinal associated with each EIP and assess direct handling or broader AOT re-entry one site at a time while preserving guest behavior.