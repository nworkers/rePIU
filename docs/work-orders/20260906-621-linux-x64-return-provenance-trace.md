# Task 621 작업 지시서: Linux x64 return target provenance trace

## 한국어

### 작업

1. `REPIU_LINUX_X64_RETURN_TRACE` 출력에 resolver target, producer site,
   post-RET guest ESP를 함께 기록합니다.
2. resolved return의 consumed slot(`guest_esp - 4`)에 해당하는 기존
   stack-write ring record를 함께 기록합니다.
3. 512개를 초과하는 runtime trace sequence를 보존하도록 x64 diagnostic ring을
   16384개로 확장합니다.
4. 기존 frame metadata와 producer tag를 재사용하고 control-flow 동작은
   변경하지 않습니다.
5. core probe와 `pumpit2a` 재현을 수행합니다.
6. 새 producer site를 AOT map 및 원본 bytes와 대조해 다음 fault 원인을
   분석 문서와 작업 로그에 남깁니다.

### 제한

* return target을 추측하거나 수정하지 않습니다.
* dynamic AOT 활성화 정책과 guest register state를 변경하지 않습니다.
* trace가 없을 때 emitted bytes와 출력은 기존과 같아야 합니다.

### 완료 조건

* `core_probe_failures=0`.
* `source=0x011A643A`의 producer site와 guest ESP가 trace에 나타납니다.
* 새 frontier의 확인 상태와 다음 작업 방향이 문서화됩니다.

## English

### Work

1. Record resolver target, producer site, and post-return guest ESP in
   `REPIU_LINUX_X64_RETURN_TRACE` output.
2. Record existing stack-write ring entries matching the resolved return's
   consumed slot (`guest_esp - 4`).
3. Expand the x64 diagnostic ring to 16384 records so a run exceeding 512
   writes retains the relevant history.
4. Reuse existing frame metadata and producer tags without changing control
   flow.
5. Run the core probe and reproduce `pumpit2a`.
6. Compare the producer site with its AOT map and original bytes, recording the
   next fault cause in analysis and the work log.

### Limits

* Do not guess or modify the return target.
* Do not change dynamic AOT policy or guest register state.
* With tracing disabled, emitted bytes and output must remain unchanged.

### Done criteria

* `core_probe_failures=0`.
* The trace identifies producer site, guest ESP, and consumed-slot writer for
  `source=0x011A643A`.
* The new frontier and next direction are documented.
