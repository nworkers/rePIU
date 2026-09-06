# Task 624 작업 지시서: Linux x64 guest write provenance

## 한국어

### 작업

1. target-address parser와 bounded write-trace 출력 모듈을 추가한다.
2. 초기 guest page write-watch 설치 시 선택 target page를 추가한다.
3. AOT write fault/completion에서 선택 주소와 겹치는 write를 기록한다.
4. HLE byte/word/dword write helper에서도 선택 주소와 겹치는 write를
   기록한다.
5. selected diagnostic page가 `ReleaseUnneededAotGuestPageWatches`에서
   제거되지 않게 한다.
6. 즉시 출력은 제한하고 최근 writer ring을 unhandled fault에서 직접 출력하여
   긴 write sequence의 마지막 writer를 보존한다.
7. core probe 및 `pumpit2a` 재현으로 첫/마지막 writer와 다음 fault를 확인한다.
8. 정적 object/file 매핑과 runtime writer 결과를 분석 문서와 작업 로그에
   반영한다.

### 제한

* `REPIU_GUEST_WRITE_TRACE`가 없으면 기존 output과 실행 semantics를
  변경하지 않는다.
* guest write 값, target, register, fault 처리 정책을 수정하지 않는다.
* 선택 page의 진단용 RX 보호와 기존 single-step 복구 경로만 사용한다.
* writer를 찾지 못해도 target을 보정하거나 guest fault를 무시하지 않는다.

### 완료 조건

* `core_probe_failures=0`.
* 선택 주소에 대한 native/HLE write event가 bounded output으로 기록된다.
* target page writer와 `0x011A6440` fault frontier가 evidence로 구분된다.

## English

### Work

1. Add a target-address parser and bounded write-trace output module.
2. Add the selected target page during initial guest page write-watch setup.
3. Record writes overlapping the selected address in AOT write fault and
   completion paths.
4. Record overlapping writes in HLE byte/word/dword write helpers.
5. Keep the selected diagnostic page from being removed by
   `ReleaseUnneededAotGuestPageWatches`.
6. Limit immediate output and dump the recent writer ring directly from the
   unhandled-fault path so a long write sequence retains its final writer.
7. Verify the first and final writer and next fault with the core probe and
   `pumpit2a` reproduction.
8. Record static object/file mapping and runtime-writer evidence in the
   analysis document and work log.

### Limits

* With `REPIU_GUEST_WRITE_TRACE` absent, preserve existing output and execution
  semantics.
* Do not modify guest write values, targets, registers, or fault policy.
* Use only diagnostic RX protection and the existing single-step recovery path
  for the selected page.
* Do not repair the target or suppress the guest fault if the writer remains
  unknown.

### Done criteria

* `core_probe_failures=0`.
* Native/HLE write events for the selected address appear with bounded output.
* Evidence distinguishes the first and final target-page writer from the
  `0x011A6440` fault frontier.
