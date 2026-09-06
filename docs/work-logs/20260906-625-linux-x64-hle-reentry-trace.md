# Task 625 작업 로그: Linux x64 HLE 이후 재진입 상태 추적

## 한국어

### 수행 내용

* `REPIU_AOT_HLE_REENTRY_TRACE=<guest-address>`를 추가했습니다.
* `TryResumeAotAfterHandledHle`에서 pending, current EIP, cache hit,
  immediate span safety, post-HLE gate, translation 결과를 선택 주소에
  한정해 기록하도록 했습니다.
* span safety 거부 사유를 `decode`, `hle-boundary`, `unreadable` 등으로
  구분했습니다.
* trace가 없을 때 기존 실행 정책과 출력은 유지했습니다.

### 검증

빌드:

```text
cmake --build build/linux_x64_repiu --target repiu_core_probe repiu -j 2
```

코어 프로브:

```text
core_probe_total=24
core_probe_failures=0
core_probe_all=true
```

재현:

```text
REPIU_AOT_HLE_REENTRY_TRACE=0x011A643F \
./build/linux_x64_repiu/repiu pumpit2a
```

주요 결과:

* HLE 직후 current EIP는 `0x011A6440`, pending은 `1`이었습니다.
* current EIP의 cache hit는 `0x200611A5`였습니다.
* immediate re-entry span은 `decode` 사유로 거부되었습니다.
* post-HLE translation gate는 이 경로에서 호출되지 않았습니다.
* 기존 fault는 `0x011A6440`, `EAX=0x37016BE9`에서 유지되었습니다.
* `REPIU_DBT_POST_HLE_TRANSLATE=1` A/B에서도 동일한 결과였으며,
  이는 cache hit/span unsafe 분기가 gate보다 먼저 실행되기 때문입니다.

### 결론 및 다음 작업

이번 작업으로 HLE 재진입 정책의 실제 분기가 확인되었습니다. cache miss나
post-HLE translation을 추측해 수정할 근거는 없습니다. 다음 작업은
`0x011A643A`를 반환하는 x64 return thunk가 전달하는 guest register/frame과
HLE 이후 single-step 경로의 register 보존을 추적하여 `EAX=0x37016BE9`가
어디서 확정되는지 확인해야 합니다.

## English

### Work performed

* Added `REPIU_AOT_HLE_REENTRY_TRACE=<guest-address>`.
* Restricted tracing in `TryResumeAotAfterHandledHle` to the selected address,
  covering pending state, current EIP, cache hit, immediate span safety,
  post-HLE gate, and translation result.
* Added span-safety reason labels such as `decode`, `hle-boundary`, and
  `unreadable`.
* Preserved the existing execution policy and output when tracing is unset.

### Verification

Build:

```text
cmake --build build/linux_x64_repiu --target repiu_core_probe repiu -j 2
```

Core probe:

```text
core_probe_total=24
core_probe_failures=0
core_probe_all=true
```

Reproduction:

```text
REPIU_AOT_HLE_REENTRY_TRACE=0x011A643F \
./build/linux_x64_repiu/repiu pumpit2a
```

Key results:

* Current EIP after HLE was `0x011A6440` with pending state `1`.
* The current EIP had a cache hit at `0x200611A5`.
* Immediate re-entry span safety rejected the span with reason `decode`.
* The post-HLE translation gate was not reached on this path.
* The existing fault remained at `0x011A6440` with `EAX=0x37016BE9`.
* Enabling `REPIU_DBT_POST_HLE_TRANSLATE=1` produced the same result because
  the cache-hit/span-unsafe branch precedes that gate.

### Conclusion and next task

The actual HLE re-entry branch is now confirmed. There is no evidence to
speculatively change the cache-miss or post-HLE translation policy. The next
task should trace the guest register/frame delivered by the x64 return thunk
for `0x011A643A` and register preservation across HLE single-step re-entry to
locate where `EAX=0x37016BE9` becomes fixed.
