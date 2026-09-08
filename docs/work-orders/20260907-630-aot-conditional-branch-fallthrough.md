# Task 630 작업 지시: AOT 조건 분기 fallthrough edge

설계: [20260907-630](../design/20260907-630-aot-conditional-branch-fallthrough.md)

## 한국어

### 범위

`src/runtime/aot_code_cache.cpp`의 `BuildAotCodeCacheImage`에서 조건 분기로
끝나는 블록의 not-taken edge를 필요할 때 emit합니다. 진단이 아니라 실행
동작을 고치는 작업입니다.

### 구현 단계

1. 블록 루프 앞에 pending fallthrough 상태 세 개를 둡니다: 활성 여부, source
   guest 주소, target guest 주소.
2. `E9`와 `kBlockFallthrough` fixup을 붙이는 lambda를 하나 만들고, 기존
   `kCopy` 경로도 그것을 쓰게 합니다.
3. 내부 루프에서 중복 제거 검사를 `emplace` 이전의 `find`로 바꿉니다. 이미
   emit된 주소면 pending을 건드리지 않고 `continue`합니다.
4. 실제로 emit할 명령 직전에 pending을 해소합니다. guest 주소가 target과
   같으면 아무것도 emit하지 않고, 다르면 `E9`를 emit합니다. 어느 쪽이든
   pending을 끕니다. 이 처리는 `cache_offset`을 읽기 전에 해야 합니다.
5. 블록 종료 처리를 다음과 같이 바꿉니다.
   * 블록이 비었거나 마지막 명령이 emit되지 않았으면 기존처럼 건너뜁니다.
   * 마지막 명령이 조건 분기이고 그 슬롯의 첫 바이트가 `0xCC`가 아니면
     pending을 설정합니다.
   * 마지막 명령이 `kCopy`이면 기존 규칙을 그대로 유지합니다.
6. 블록 루프가 끝난 뒤 pending이 남아 있으면 emit합니다.
7. `src/tools/aot_probe/long_mode_emission_probe.cpp`에 항목을 추가합니다.
   * 인접 fallthrough: `kBlockFallthrough` fixup이 생기지 않습니다.
   * 앞쪽에 이미 emit된 fallthrough: 해소된 `kBlockFallthrough` fixup이 그
     cache offset을 가리킵니다.
   * `RunLongModeEmissionProbe` 집계에 새 항목을 넣습니다.

### 금지 사항

* 조건 분기마다 무조건 `E9`를 붙이지 않습니다.
* fallthrough가 물리적으로 다음인 image의 바이트를 바꾸지 않습니다.
* fixup 해소 규칙, `kCopy` fallthrough 규칙, timer safe point 규칙 변경 금지.
* return target 보정이나 stack slot 수정 금지.

### 검증

1. `cmake --build build/linux_x64 --target repiu_core_probe repiu -j 4`
2. `./build/linux_x64/repiu_core_probe` — `core_probe_failures=0`과
   `long_mode_emission_all=true` 확인.
3. `./build/linux_x64/repiu pumpit2a` — `0x011A6440` fault가 사라지는지 확인.
4. 실행이 멈추는 새 지점을 기록합니다.
5. Task 629의 map dump로 `0x0102A06C`에 `kBlockFallthrough` fixup이 생겼는지
   확인합니다.

### 문서

* `docs/analysis/linux-port-frontier.md`에 Task 630 절을 추가합니다.
* `docs/work-logs/20260907-630-aot-conditional-branch-fallthrough.md`를 씁니다.
* `ARCHITECTURE.md`의 AOT emitter 설명에 fallthrough 규칙을 반영합니다.

## English

### Scope

In `BuildAotCodeCacheImage` in `src/runtime/aot_code_cache.cpp`, emit the
not-taken edge of a block ending in a conditional branch when it is needed. This
changes execution behavior; it is not a diagnostic.

### Implementation steps

1. Add three pending-fallthrough values before the block loop: active flag,
   source guest address, target guest address.
2. Add one lambda that appends the `E9` and its `kBlockFallthrough` fixup, and
   use it from the existing `kCopy` path too.
3. In the inner loop, replace the deduplication `emplace` check with a `find`
   before it. An already emitted address `continue`s without touching the
   pending.
4. Resolve the pending immediately before emitting an instruction: emit nothing
   if the guest address equals the target, otherwise emit the `E9`. Clear the
   pending either way. This must happen before `cache_offset` is read.
5. Change the block-close handling:
   * Skip as today when the block is empty or its last instruction was not
     emitted.
   * Set the pending when the last instruction is a conditional branch whose
     slot does not begin with `0xCC`.
   * Keep the existing rule for a `kCopy` last instruction.
6. Emit any pending left after the block loop.
7. Add items to `src/tools/aot_probe/long_mode_emission_probe.cpp`.
   * Adjacent fallthrough: no `kBlockFallthrough` fixup appears.
   * Fallthrough already emitted earlier: a resolved `kBlockFallthrough` fixup
     points at that cache offset.
   * Include the new items in the `RunLongModeEmissionProbe` aggregate.

### Prohibited

* Unconditionally appending an `E9` after every conditional branch.
* Changing the bytes of an image whose fallthrough is physically next.
* Changing fixup resolution, the `kCopy` fallthrough rule, or the timer
  safe-point rule.
* Repairing return targets or editing stack slots.

### Verification

1. `cmake --build build/linux_x64 --target repiu_core_probe repiu -j 4`
2. `./build/linux_x64/repiu_core_probe` — confirm `core_probe_failures=0` and
   `long_mode_emission_all=true`.
3. `./build/linux_x64/repiu pumpit2a` — confirm the `0x011A6440` fault is gone.
4. Record the new point where execution stops.
5. Use the Task 629 map dump to confirm `0x0102A06C` now carries a
   `kBlockFallthrough` fixup.

### Documentation

* Add a Task 630 section to `docs/analysis/linux-port-frontier.md`.
* Write `docs/work-logs/20260907-630-aot-conditional-branch-fallthrough.md`.
* Reflect the fallthrough rule in the AOT emitter description in
  `ARCHITECTURE.md`.
