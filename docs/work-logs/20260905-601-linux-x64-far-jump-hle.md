# 작업 로그 20260905-601 — Linux x64 guest `66 EA` far jump HLE

설계: [20260905-601](../design/20260905-601-linux-x64-far-jump-hle.md)
작업 지시: [20260905-601](../work-orders/20260905-601-linux-x64-far-jump-hle.md)

## 결과

Task 600에서 확인한 guest 명령 `66 EA 04 00 2C 00`을 Linux x64 long
mode에서 selector table 기반 linear EIP 전환으로 처리했습니다. HLE는
`offset16`과 `selector16`을 명령어에서 읽고 `TranslateSelectorOffset`으로
범위와 descriptor 상태를 검증한 뒤, 성공할 때만 EIP를 갱신합니다. ESP와
EFLAGS는 변경하지 않습니다.

`002C:0004`는 core probe에서 `0x01100004`로 변환되며, 없는 selector와
limit 초과 offset은 모두 거부됩니다.

## 검증

빌드:

```text
cmake --build build/linux_x64_debug --target repiu repiu_core_probe -j 2
[100%] Built target repiu
[100%] Built target repiu_core_probe
```

core probe:

```text
far_jump_selector_translation=true,valid=true,missing=true,out_of_limit=true
core_probe_total=22
core_probe_failures=0
core_probe_all=true
```

probe-success runtime에서는 이전 frontier인 `0x010F016B`의 far-jump SIGILL이
재발하지 않았습니다. target 경로의 guest `INT3` `0x01100042`도 한 번
소비되었습니다. 이후 host AOT `CC/UD2` 지점에서 SIGTRAP/SIGILL이 발생해
실행이 종료되었으며, 이 지점은 다음 작업에서 AOT 재진입 sentinel과 실제
미지원 guest 경로를 분리해 조사할 새 frontier로 남겼습니다.

## 상태

* `66 EA ptr16:16` Linux x64 HLE: 완료
* selector 유효성·범위 검증: 완료
* 이전 `0x010F016B` SIGILL: 해결
* `INT 31h AX=1E7Fh` 실제 성공 ABI: 미확정; probe-success는 진단 전용
* 이후 host `CC/UD2` 재진입 지점: 미해결; 다음 작업 대상

---

# Work log 20260905-601 — Linux x64 guest `66 EA` far-jump HLE

Design: [20260905-601](../design/20260905-601-linux-x64-far-jump-hle.md)
Work order: [20260905-601](../work-orders/20260905-601-linux-x64-far-jump-hle.md)

## Result

Implemented the Task 600 frontier `66 EA 04 00 2C 00` as a selector-table-based
linear EIP transfer for Linux x64 long mode. The HLE reads `offset16` and
`selector16` from the instruction, validates them with
`TranslateSelectorOffset`, and updates EIP only after successful translation.
ESP and EFLAGS remain unchanged.

The core probe translates `002C:0004` to `0x01100004` and rejects both a
missing selector and an offset beyond the descriptor limit.

## Verification

Build:

```text
cmake --build build/linux_x64_debug --target repiu repiu_core_probe -j 2
[100%] Built target repiu
[100%] Built target repiu_core_probe
```

Core probe:

```text
far_jump_selector_translation=true,valid=true,missing=true,out_of_limit=true
core_probe_total=22
core_probe_failures=0
core_probe_all=true
```

With the probe-success runtime setting, the former far-jump SIGILL at
`0x010F016B` did not recur. The guest `INT3` at `0x01100042` on the target path
was also consumed once. Execution then terminated at a host AOT `CC/UD2`
location with SIGTRAP/SIGILL. That location is recorded as the next frontier;
the next task must distinguish an AOT re-entry sentinel from an actual
unsupported guest path.

## Status

* `66 EA ptr16:16` Linux x64 HLE: complete
* Selector validity and range checks: complete
* Former `0x010F016B` SIGILL: resolved
* Actual success ABI of `INT 31h AX=1E7Fh`: unresolved; probe-success is
  diagnostic-only
* Subsequent host `CC/UD2` re-entry point: unresolved; next task
