# Task 666 작업 지시: Linux x64 direct-edge 동적 실행 확인

## 한국어

### 작업 범위

1. Task 665의 정적 incoming candidate `0x010EFF2A -> 0x010F0232`를
   기존 execution sentinel로 동적 확인합니다.
2. 같은 실행에서 `0x010F0232` HLE 전후 ESP와 `0x010F0237 RET` zero-return을
   수집합니다.
3. epilogue의 `+0x14` 변화와 반환 슬롯의 zero 상태를 구분합니다.
4. 주소별 보정이나 resolver 정책 변경 없이 설계·분석·작업 로그를 갱신하고
   하나의 Git commit으로 남깁니다.

### 실행 절차

- Linux x64 Debug binary를 사용합니다.
- source sentinel filter:
  `REPIU_EXECUTION_TRACE_START=0x000EFF2A`
- HLE boundary filter:
  `REPIU_AOT_HLE_REENTRY_TRACE=0x010F0232`
- `REPIU_LINUX_X64_RETURN_FRAME_TRACE=1`을 사용합니다.
- bounded timeout을 사용하고 결과에서 다음 prefix를 확인합니다.
  `repiu-watch`, `repiu-hle-reentry`, `repiu-x64-return-frame`,
  `repiu-fault`

### 완료 조건

- source candidate의 실제 동적 도달 여부가 확인됩니다.
- source cache hit와 target HLE re-entry가 같은 실행에서 연결됩니다.
- ESP의 `0x0158C84C -> 0x0158C860` 변화가 기록됩니다.
- `RET` 반환 슬롯이 zero이고 기존 fail-closed `SIGTRAP`으로 끝남을
  확인합니다.
- 결과가 “동적 provenance 확인”과 “근본 원인 미확정”으로 문서화됩니다.
- 소스 코드 변경 없이 설계·작업지시·작업로그·누적분석이 커밋됩니다.

## English

### Scope

1. Dynamically confirm Task 665's static incoming candidate
   `0x010EFF2A -> 0x010F0232` with the existing execution sentinel.
2. Capture HLE-before/HLE-after ESP and the `0x010F0237 RET` zero-return in
   the same run.
3. Separate the epilogue's `+0x14` stack effect from the missing return slot.
4. Update design, analysis, and work-log documents without adding an
   address-specific correction, then leave one Git commit.

### Procedure

- Use the Linux x64 Debug binary.
- Set `REPIU_EXECUTION_TRACE_START=0x000EFF2A`.
- Set `REPIU_AOT_HLE_REENTRY_TRACE=0x010F0232`.
- Set `REPIU_LINUX_X64_RETURN_FRAME_TRACE=1`.
- Use a bounded timeout and inspect the source watch, HLE re-entry,
  return-frame, and final fault records.

### Completion criteria

- Dynamic arrival at the static source candidate is confirmed.
- The source cache hit and target HLE re-entry are linked in one run.
- ESP `0x0158C84C -> 0x0158C860` is recorded.
- The `RET` slot is zero and the existing fail-closed `SIGTRAP` is reproduced.
- The result is documented as dynamic provenance confirmed, root cause unresolved.
- Design, work order, work log, and cumulative analysis are committed without
  source changes.
