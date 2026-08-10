# 20260811-467 AOT byte-guard jump table 작업 지시 / AOT Byte-Guard Jump Table Work Order

설계: [20260811-467-aot-byte-guard-jump-table.md](../design/20260811-467-aot-byte-guard-jump-table.md)

## 한국어

- [x] pumpito hotspot과 기존 bounded jump-table matcher의 차이를 확인합니다.
- [x] target 및 guest 주소에 독립적인 byte-guard 전달 설계를 작성합니다.
- [x] low-byte compare와 `and r32,0xff` 정규화 관계를 검증하는 planner matcher를 구현합니다.
- [x] 기존 방문 순서 독립 재분류와 native jump-table emitter를 재사용합니다.
- [x] synthetic 성공 및 fail-closed probe를 추가합니다.
- [x] pumpito PIU.EXE plan과 Win32 x86 Release build를 검증합니다.
- [x] architecture, 누적 분석과 작업 로그를 갱신합니다.
- [x] 변경을 커밋합니다.

## English

- [x] Confirm the difference between the pumpito hotspot and the existing bounded jump-table matcher.
- [x] Design byte-guard propagation independent of target and guest address.
- [x] Implement planner matching that validates the relationship between a low-byte compare and
  `and r32,0xff` normalization.
- [x] Reuse the existing visit-order-independent reclassification and native jump-table emitter.
- [x] Add synthetic success and fail-closed probes.
- [x] Verify the pumpito PIU.EXE plan and a Win32 x86 Release build.
- [x] Update architecture, cumulative analysis, and the work log.
- [x] Commit the changes.
