# Task 624 작업 로그: Linux x64 guest write provenance

## 한국어

### 수행 내용

* `REPIU_GUEST_WRITE_TRACE=0x011A643A`를 추가하고 선택된 guest page를
  초기 AOT write-watch에 포함했습니다.
* native write fault/completion과 HLE guest write를 같은 target 기준으로
  기록했습니다.
* 긴 trace가 실행을 과도하게 지연시키지 않도록 첫 32건만 즉시 출력하고,
  unhandled fault 시 최근 64건을 async-signal-safe `write(2)` 경로로
  출력하도록 했습니다.

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

실행 재현:

```text
REPIU_GUEST_WRITE_TRACE=0x011A643A \
REPIU_AOT_GUEST_MAP_TRACE=0xF2469 \
REPIU_AOT_DYNAMIC_TRACE=0x011A643A \
./build/linux_x64_repiu/repiu pumpit2a
```

주요 결과:

* 첫 writer: guest source `0x010F29FA`, execution cache `0x2000B13D`,
  target `0x011A643A`, 1-byte 결과 `00`.
* 마지막 writer: 총 2642개 event 뒤 guest source `0x010F2469`, execution
  cache `0x200140A3`, `EDX=0x011A643A`, `EBX=0x016BE90D`, 결과 bytes
  `0D E9 6B 01`.
* source map entry `0x010F2469`의 bytes는 `67 89 1A`이며, 이는
  `MOV [EDX],EBX`입니다.
* dynamic raw/plan/image bytes는 `0D E9 6B 01 37`로 writer 결과와
  일치했습니다.
* `PUSH ES` HLE는 selector `0x0024`를 `0x0158CC44`에 기록하고
  `0x011A6440`으로 진행했습니다.
* 최종 fault는 기존과 같이 `0x011A6440`의 `00 00`에서
  `EAX=0x37016BE9` access로 발생했습니다.

### 결론 및 다음 작업

target page가 0에서 동적 code fragment로 바뀌는 writer 경로는 guest AOT
실행으로 확인되었습니다. writer가 target bytes를 잘못 만들었거나 dynamic
append가 다른 bytes를 사용했다는 가설은 기각합니다. Linux x64의 남은 문제는
정상적인 `PUSH ES` HLE 이후 `0x011A6440`을 어떻게 재진입하는지, 그리고
그 시점의 `EAX=0x37016BE9`가 어떤 guest 값 경로에서 왔는지입니다.

## English

### Work performed

* Added `REPIU_GUEST_WRITE_TRACE=0x011A643A` support and placed the selected
  guest page in the initial AOT write-watch set.
* Recorded native write faults/completions and HLE guest writes against the
  same target.
* Limited immediate output to the first 32 events and dumped the most recent
  64 records through async-signal-safe `write(2)` on an unhandled fault, so a
  long trace does not stall the reproduction.

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
REPIU_GUEST_WRITE_TRACE=0x011A643A \
REPIU_AOT_GUEST_MAP_TRACE=0xF2469 \
REPIU_AOT_DYNAMIC_TRACE=0x011A643A \
./build/linux_x64_repiu/repiu pumpit2a
```

Key results:

* First writer: guest source `0x010F29FA`, execution cache `0x2000B13D`,
  target `0x011A643A`, one-byte result `00`.
* Final writer: after 2642 events, guest source `0x010F2469`, execution cache
  `0x200140A3`, `EDX=0x011A643A`, `EBX=0x016BE90D`, result bytes
  `0D E9 6B 01`.
* The source map entry at `0x010F2469` is `67 89 1A`, which is
  `MOV [EDX],EBX`.
* Dynamic raw/plan/image bytes are `0D E9 6B 01 37`, matching the writer.
* `PUSH ES` HLE writes selector `0x0024` to `0x0158CC44` and advances to
  `0x011A6440`.
* The final fault remains the existing `00 00` at `0x011A6440` accessing
  `EAX=0x37016BE9`.

### Conclusion and next task

The guest AOT write path that changes the target page from zero into the
dynamic code fragment is confirmed. The hypotheses that the writer creates
wrong target bytes or that dynamic append uses different bytes are rejected.
The remaining Linux x64 issue is how execution re-enters after the legitimate
`PUSH ES` HLE at `0x011A643F`, and which guest value path produces
`EAX=0x37016BE9` there.
