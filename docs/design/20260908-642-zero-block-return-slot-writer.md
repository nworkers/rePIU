# Task 642 설계: zero-block 반환 슬롯 writer 확인

## 한국어

Task 641에서 `0x010F1E56 RET`가 stack 슬롯 `0x0158CC84`의
`0x011A8E10`을 target으로 소비함을 확인했습니다. 기존
`REPIU_GUEST_WRITE_TRACE`를 그 슬롯에 적용하여 정확한 destination write fault와 guest
source·레지스터를 찾고, AOT guest map으로 source instruction bytes를 교차 확인합니다.

이 작업은 기존 진단 기능만 사용하며 실행 제어를 변경하지 않습니다. writer가
확인되면 다음 분석 경계를 값 생성이 아니라 writer 이후 stack 균형으로 이동합니다.

## English

Task 641 confirmed that `RET` at `0x010F1E56` consumes `0x011A8E10` from stack
slot `0x0158CC84`. Apply the existing `REPIU_GUEST_WRITE_TRACE` to that slot to
identify an exact-destination write fault with its guest source and registers,
then cross-check the source instruction bytes through the AOT guest map.

This task uses existing diagnostics only and does not change execution control.
Once the writer is confirmed, move the next boundary from value production to
stack balance after the writer.
