# Task 642 작업 지시: zero-block 반환 슬롯 writer 확인

설계: [20260908-642](../design/20260908-642-zero-block-return-slot-writer.md)

## 한국어

1. `REPIU_GUEST_WRITE_TRACE=0x0158CC84`로 실제 실행을 캡처합니다.
2. exact destination fault의 guest source와 레지스터를 확인합니다.
3. source AOT map bytes로 writer 명령을 교차 검증합니다.
4. 분석 문서와 작업 로그를 갱신하고 커밋합니다.

## English

1. Capture real execution with `REPIU_GUEST_WRITE_TRACE=0x0158CC84`.
2. Identify the exact-destination fault's guest source and registers.
3. Cross-check the writer instruction through source AOT map bytes.
4. Update the analysis and work log, then commit the task.
