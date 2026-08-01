# 작업 로그: Single-step hotspot 검토 / Work log: single-step hotspot review

## 결과

- hotspot 계측은 trace-owned single-step 85,713건을 기록했습니다.
- HLE 결과는 66,152건·3,641,953,045 cycle(약 98.43%)이고, plain TF 재설정은 19,549건·57,878,284 cycle(약 1.56%)입니다.
- post-HLE AOT 복귀는 82.06% 성공했고, 17.94%의 span-safety 거절은 주원인으로 확정하지 않았습니다.
- 구현 변경과 빌드는 수행하지 않았습니다. 다음 단계는 상위 HLE/AOT-resume EIP의 명령과 ordinal 귀속 분석입니다.

## English

- The hotspot profile recorded 85,713 trace-owned single steps.
- HLE outcomes were 66,152 calls and 3,641,953,045 cycles (about 98.43%); plain TF re-arm was 19,549 calls and 57,878,284 cycles (about 1.56%).
- Post-HLE AOT re-entry succeeded 82.06% of the time; its 17.94% span-safety rejection is not confirmed as the main cause.
- No implementation or build was performed. The next step is instruction and ordinal attribution for the leading HLE/AOT-resume EIPs.