# 20260727-329 작업 지시: arena 스냅샷 제거 / Work order: Arena snapshot elimination

설계: [docs/design/20260727-329-arena-snapshot-elimination-options.md](../design/20260727-329-arena-snapshot-elimination-options.md)

## 한국어

### 목표

`AppendWin32DynamicAotTranslation`이 진입할 때마다 복사하는 guest arena 전체 스냅샷
(140,341,248바이트, append의 56.96%)을 제거합니다. 설계의 **옵션 1**을 채택해 번역기가
같은 프로세스 안의 **live arena를 직접 참조**하게 합니다. 선행 조건인 "guest 외 스레드의
arena 쓰기 없음"은 설계 8절에서 감사로 확인했습니다.

**보이는 범위가 133.8MB로 동일하므로 plan은 바이트 단위로 보존되어야 합니다.** 이것이
이 작업의 성패 기준이며, 성능 수치보다 우선합니다.

### 구현 항목

1. `include/repiu/runtime/runtime_memory.h`
   - `RelocatedRuntimeObject`에 additive 필드 `external_bytes`,
     `external_byte_count`를 추가합니다. 기존 사용처는 두 필드를 설정하지 않으므로
     동작이 바뀌지 않습니다.
   - **수명·불변성 계약을 구조체 주석으로 남깁니다.** 참조 구간 동안 매핑이 유지되고
     내용이 바뀌지 않아야 하며, 현재 그 보장은 **동기 rendezvous에 의존**합니다.
     번역을 비동기로 바꾸려면 이 가정을 먼저 되돌려야 한다는 경고를 함께 남깁니다.
   - 읽기 경로가 두 표현을 구분하지 않도록 inline 접근자
     `RelocatedRuntimeObjectBytes`, `RelocatedRuntimeObjectByteCount`를 추가합니다.
2. `src/runtime/aot_translation_plan.cpp`
   - `FindBytes`가 접근자를 쓰도록 바꿉니다. 범위 판정 규칙(`offset + bytes <= size`)은
     그대로 둡니다.
3. `src/runtime/image_address.cpp`
   - `BuildRelocatedImageByteWindow`도 같은 접근자를 쓰게 합니다. 지금은 외부 뷰
     object를 받지 않지만, 두 번째 읽기 경로가 조용히 빈 결과를 내는 상태를 남기지
     않습니다.
4. `src/platform/win32/aot_code_cache_win32.cpp`
   - 스냅샷 3단계(`resize` zero-fill, `ReadProcessMemory`, 소멸자 해제)를 삭제하고
     `external_bytes = runtime_base`, `external_byte_count = runtime_size`로
     대체합니다.
   - 첫 호출 1회에 한해 `VirtualQuery`로 `[runtime_base, +runtime_size)` 전 구간이
     `MEM_COMMIT`이고 읽기 가능한 보호인지 확인하고, 실패하면 append를 거절합니다
     (설계 9절, 삭제되는 `ReadProcessMemory` 실패 경로를 대신하는 fail-safe).
   - `arena_snapshot_cycles` 계측 구간은 **유지**합니다. A/B에서 값이 무너지는 것이
     이번 변경의 직접 증거입니다.
   - `append_scale.snapshot_bytes`는 실제 복사량이므로 `0`을 기록합니다.
5. `src/tools/aot_probe/arena_view_probe.{h,cpp}` (신규), `CMakeLists.txt`,
   `src/tools/aot_probe/main.cpp`
   - 소유 복사본(변경 전 표현)을 oracle로 두고 외부 뷰와 **의미 동등성**을 검증합니다.

### 안전 조건

- guest에게 보이는 실행 순서, 반환값, `result` 내용을 바꾸지 않습니다.
- `RelocatedRuntimeObject`는 플랫폼 공용 구조체이므로 변경은 **additive만** 허용합니다
  (설계 F6). 정적 로더 경로와 probe가 같은 구조를 씁니다.
- 실패 경로에서도 phase 누적이 유지되어야 합니다(Task 328 계약).
- 이 함수는 워커 스레드 전용이므로 **원자 연산이나 잠금을 추가하지 않습니다.**
- `repiu_aot_probe`가 이 함수를 호출하므로 probe 경로가 계속 통과해야 합니다.

### 검증

1. `powershell -File scripts/build_win32_x86.ps1`
2. `repiu_aot_probe build/runtime_mounts/pumpit1/PIU/PIU.EXE` 전체 통과.
   신규 `arena_view_*` 그룹 포함.
3. 신규 probe가 검증할 동등성 축:
   - plan 스칼라 전 필드(`block_count`, `instruction_count`,
     `outside_image_target_count`, kind별 카운터, `decode_failure_count` 등)
   - block/instruction 스트림 전체(주소, kind, 길이, **원본 바이트**, jump table target)
   - emit된 `AotCodeCacheImage`의 `bytes`와 `address_map` **바이트 단위 일치**
   - 외부 뷰가 **live**임(빌드 사이 guest 바이트를 바꾸면 결과가 따라 바뀜)
   - 범위 밖 target 처리가 양쪽에서 동일함
4. 실행 검증(게임 60초 A/B)은 사용자 확인 후 별도로 수행합니다.
   기대 신호: `arena-snapshot` 단계 붕괴, `placement` 동반 감소, `snapshot-bytes=0`,
   EEPROM hash 일치, malformed 0.

---

## English

### Goal

Remove the full guest-arena snapshot (140,341,248 bytes, 56.96% of one append) that
`AppendWin32DynamicAotTranslation` copies on entry, adopting the design's Option 1 so the
translator references the live arena directly in the same process. The prerequisite — that no
thread other than the guest writes into the arena — was audited in design section 8. Because the
visible range stays the same 133.8MB, the plan must be preserved byte for byte; that, not the
performance number, is the pass condition.

### Implementation

Add the additive fields `external_bytes` and `external_byte_count` to `RelocatedRuntimeObject`
together with the lifetime and immutability contract in the structure comment, including the
warning that the guarantee currently rests on the synchronous rendezvous and must be restored
before translation can become asynchronous, plus inline `RelocatedRuntimeObjectBytes` and
`RelocatedRuntimeObjectByteCount` accessors so read paths do not care which representation an
object uses. Move `FindBytes` and `BuildRelocatedImageByteWindow` onto those accessors, keeping
the range rule unchanged. In `AppendWin32DynamicAotTranslation`, replace the three snapshot steps
(zero-filling resize, `ReadProcessMemory`, destructor free) with the external view, verify once
per process with `VirtualQuery` that the whole arena range is committed and readable and refuse
the append otherwise, keep the `arena_snapshot_cycles` region so the A/B shows it collapse, and
record `snapshot_bytes` as 0 because nothing is copied. Add an `arena_view` probe that treats the
owning copy as the oracle.

### Safety

Guest-visible ordering, return values, and `result` contents stay unchanged. `RelocatedRuntimeObject`
is platform-neutral and shared with the static loader path and probes, so the change must be
additive (F6). Phase accumulation must survive failure paths as Task 328 established. No atomics
or locks are added, since the function runs only on the worker thread, and `repiu_aot_probe` must
keep passing because it calls this function.

### Verification

Build, then pass `repiu_aot_probe` including the new `arena_view` group, which compares every
plan scalar, the whole block and instruction stream including original bytes and jump-table
targets, and the emitted image bytes and address map byte for byte between an owning copy and an
external view, and additionally proves the view is live and that out-of-range targets are handled
identically. The 60-second in-game A/B is run separately after user confirmation; the expected
signals are a collapsed `arena-snapshot` phase, a matching drop in `placement`,
`snapshot-bytes=0`, a matching EEPROM hash, and zero malformed dispatch.
