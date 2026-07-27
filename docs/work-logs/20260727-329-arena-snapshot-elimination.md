# 20260727-329 작업 로그: arena 스냅샷 제거 / Work log: Arena snapshot elimination

설계: [20260727-329-arena-snapshot-elimination-options.md](../design/20260727-329-arena-snapshot-elimination-options.md)

작업 지시: [20260727-329-arena-snapshot-elimination.md](../work-orders/20260727-329-arena-snapshot-elimination.md)

## 한국어

### 결론 요약

설계의 **옵션 1(live arena 직접 참조)** 을 구현했습니다. 번역 1회마다 발생하던
**140,341,248바이트의 zero-fill + `ReadProcessMemory` 복사 + 해제가 모두 사라졌습니다.**

선행 조건이었던 "guest thread 외 다른 스레드가 arena에 쓰는가"는 코드 감사로
**'쓰지 않는다'로 확인**되어 옵션 2로 전환할 필요가 없었습니다.

### 선행 조건 감사 결과 (확인됨)

| 스레드 | arena 쓰기 | 근거 |
|---|---|---|
| host main (poll loop) | 없음 | 유일한 메모리 쓰기는 `WriteDosLowMemory`이며 대상은 host 소유 `DosLowMemory::bytes` 벡터입니다. |
| CD-DA worker | 없음 | CHD 섹터 → host 벡터 → `SDL_PutAudioStreamData` |
| YMZ280B worker | 없음 | 샘플 ROM → host 블록 버퍼 |
| 번역 워커 | 없음 | AOT cache(별도 `VirtualAlloc`)에만 쓰고 arena는 `VirtualProtect`만 |

Glide backend는 별도 스레드가 아니라 host main 스레드에서 guest 요청을 대행하며,
`InvokeOnHostThread`가 guest를 `host_command_complete_`까지 차단하므로 **Glide host
command 구간과 번역 rendezvous 구간은 상호 배타적**입니다. guest thread는 한 번에 한
곳에서만 차단될 수 있기 때문입니다.

부수적으로 두 가지를 확인했습니다. 저장소 전체에 `MEM_DECOMMIT`이 없고 모든
`VirtualFree`는 `MEM_RELEASE`이며, guest 페이지 보호는 `PAGE_EXECUTE_READ`,
`PAGE_EXECUTE_READWRITE`, `PAGE_READWRITE` 사이에서만 바뀝니다. 즉 실행 중 arena는
사라지지도, 읽을 수 없게 되지도 않습니다.

또한 `AotInstructionRecord::bytes`가 소유 벡터이므로 **plan은 원본 포인터를 보관하지
않습니다.** 수명 계약은 plan build 구간으로 한정됩니다.

### 구현

1. `RelocatedRuntimeObject`에 additive 필드 `external_bytes`,
   `external_byte_count`와 수명·불변성 계약 주석을 추가했습니다. 비동기 번역이
   이 가정을 깬다는 경고를 코드에 남겼습니다.
2. 읽기 경로용 inline 접근자 `RelocatedRuntimeObjectBytes`,
   `RelocatedRuntimeObjectByteCount`를 추가하고 `FindBytes`와
   `BuildRelocatedImageByteWindow`를 그 위로 옮겼습니다. 범위 판정 규칙은 그대로입니다.
3. `AppendWin32DynamicAotTranslation`의 스냅샷 3단계를 외부 뷰 설정으로 대체했습니다.
4. fail-safe로 프로세스당 1회 `VirtualQuery` 순회를 추가해 arena 전 구간이
   `MEM_COMMIT`이고 읽기 가능한지 확인하고, 실패하면 append를 거절합니다. 삭제된
   `ReadProcessMemory` 실패 경로를 대신하며 매 append 비용은 0입니다.
5. `append_scale.snapshot_bytes`는 **실제 복사 바이트**이므로 이제 `0`입니다.
   `arena_snapshot_cycles` 계측 구간은 A/B 증거로 남겼습니다.

### 검증 결과

1. Win32 x86 Debug 전체 빌드 통과(신규 warning 없음).
2. `repiu_aot_probe` 전체 통과, exit 0. 신규 `arena_view_*` 6개 항목 포함 모든 그룹
   `true`.
3. 신규 `arena_view` probe는 **소유 복사본을 oracle로** 두고 다음을 검증합니다.

| 항목 | 내용 | 결과 |
|---|---|---|
| `identical_plan` | plan 스칼라 전 필드 + block/instruction 스트림(원본 바이트 포함) + emit 이미지 `bytes`·`address_map`·`fixups` 일치 | true |
| `cfg_exercised` | 비교 대상 CFG가 call/조건분기/간접exit/범위밖 target을 실제로 포함 | true |
| `live_visible` | 빌드 사이 guest 바이트를 바꾸면 양쪽이 함께 바뀜(뷰가 stale 별칭이 아님) | true |
| `edge_refused` | 끝에서 4바이트 지점 진입 시 양쪽 모두 `outside_image_target_count=1`로 거절 | true |
| `outside_refused` | object 밖 진입을 양쪽 모두 거절 | true |
| `byte_window` | 두 번째 읽기 경로(`BuildRelocatedImageByteWindow`)도 동일 결과 | true |

4. 기존 `coherence` probe(실제 `AppendWin32DynamicAotTranslation` 호출)도 전 항목
   통과했습니다. 특히 `coherence_live_snapshot=true`는 직접 참조 경로가 append 직전
   변경된 guest 바이트(`guest[4]=0x13`)를 반영해 번역했음을 보입니다.

### 실게임 60초 측정 (aot-dbt, `REPIU_EXECUTION_TIME_PROFILE=1`)

정상 60초 timeout, exception dispatch malformed **0**, EEPROM SHA-256
`A1FC1D...52570`으로 fixture와 일치했습니다. fatal은 기존 Glide 구현 공백
(`_GRHINTS@8`, `action=continue`) 하나뿐입니다.

append 단계 비용을 **번역 1회당**으로 정규화한 비교입니다(Task 328은 139회,
이번은 239회이므로 총합 비교는 부적절합니다).

| 단계 | Task 328 회당 | Task 329 회당 | 변화 |
|---|---:|---:|---:|
| **`arena_snapshot`** | 404,524,860 | **7,970** | **-99.998%** |
| `placement` | 184,814,412 | 26,839,702 | -85.5% |
| `plan_build` | 81,728,912 | 26,907,556 | -67.1% |
| `image_emit` | 35,797,624 | 12,806,455 | -64.2% |
| `validate` | 3,089,728 | 772,181 | -75.0% |
| **append 합계** | **710,135,523** | **67,367,429** | **-90.5% (10.5배)** |

번역 단위는 비교 가능한 크기를 유지했습니다(회당 명령 1,039 → 1,058,
블록 310 → 290). 스냅샷 바이트는 140,341,248 → **0**, 최대 스냅샷 tick은
578,038,551 → 376,179입니다.

**확인됨: Task 328의 귀속 주의가 옳았습니다.** `placement` 회당 비용이 85.5%
줄었으므로 그 26.03% 중 대부분이 133.8MB 벡터 해제였습니다. 스냅샷 생애주기 전체는
회당 약 `404.5M + 158.0M = 562.5M`, 즉 append의 **약 79%** 로, Task 328이 제시한
`57~83%` 구간의 상단에 해당합니다. 다만 감소분 일부는 해제 자체가 아니라 메모리
압력 감소일 수 있으므로 79%는 상한에 가까운 추정입니다.

전체 실행 축에서는 `kAotDynamicTranslate`가 AOT transfer function 축의
**88.64%(Task 326) → 26.44%** 로 내려왔고, 60초 progress는 `9,293`(Task 328 ON) 대비
**62,566**, heartbeat는 784,320이었습니다. 다만 Tasks 325~328이 기록했듯 실행 간
편차가 크므로 배수 자체는 단일 표본의 관측치로만 취급합니다.

### 미확정 / Unresolved

* **`plan_build`·`image_emit`·`validate`가 함께 64~75% 싸진 이유는 측정하지
  않았습니다.** 유력한 설명은 번역마다 133.8MB를 할당·zero-fill·해제하던 메모리
  압력이 사라져 이후 단계가 warm·resident 페이지에서 동작한다는 것이지만, 이번
  작업에서 분리 측정하지 않았으므로 **추정**입니다.
* `plan_build`는 이제 append의 39.94%로 최대 항목입니다. 명령당 약 25,433 tick이며
  다음 후보입니다.
* 이번 표본은 1회 실행입니다. 편차가 큰 축(progress, heartbeat)은 반복 표본 없이는
  확정하지 않습니다.

---

## English

### Summary

Option 1 from the design — referencing the live arena — is implemented, removing the
140,341,248-byte zero-fill, `ReadProcessMemory` copy, and free that every translation performed.
The prerequisite was audited in code and confirmed: no thread other than the guest writes into
the arena, so the fallback to Option 2 was not needed.

### Prerequisite audit

The host main poll loop's only memory write is `WriteDosLowMemory` into the host-owned
`DosLowMemory::bytes` vector; both audio workers stay in host buffers; and the translation worker
writes only the separately allocated AOT cache, touching arena pages through `VirtualProtect`
alone. The Glide backend is host-main-thread work performed on the guest's behalf, and because
`InvokeOnHostThread` blocks the guest until `host_command_complete_`, a Glide host command and a
translation rendezvous are mutually exclusive — the guest thread can block in only one place.
The audit also established that nothing decommits (no `MEM_DECOMMIT`; every `VirtualFree` is a
teardown `MEM_RELEASE`) and that guest page protection moves only among readable values, so the
arena neither disappears nor becomes unreadable during a run. Because `AotInstructionRecord::bytes`
is an owning vector, the plan retains no pointer into the source and the lifetime contract is
bounded by the plan build.

### Implementation

`RelocatedRuntimeObject` gained the additive `external_bytes` and `external_byte_count` fields
with the lifetime and immutability contract in the comment, including the warning that
asynchronous translation would invalidate it. Read paths moved onto the new
`RelocatedRuntimeObjectBytes` and `RelocatedRuntimeObjectByteCount` accessors, keeping the range
rule identical, and `AppendWin32DynamicAotTranslation` now sets the view instead of taking a
snapshot. A single `VirtualQuery` walk per process replaces the fail-safe that
`ReadProcessMemory`'s failure return provided, and `snapshot_bytes` now reports 0 because that is
what is copied.

### Verification

The full Win32 x86 Debug build and `repiu_aot_probe` pass with exit 0, including the new
`arena_view` group. That probe treats the owning copy as the oracle and requires identical plan
scalars, block and instruction streams including original bytes, and emitted image bytes, address
map, and fixups; it also proves the CFG under test exercises calls, conditional branches, an
indirect exit, and an out-of-range target, that a mid-test guest byte change is visible through
the view, that an entry within one instruction length of the end is refused identically, that an
entry outside the object is refused, and that the second reader windows both representations the
same way. The existing coherence probe, which calls `AppendWin32DynamicAotTranslation` for real,
also passes, and its `coherence_live_snapshot` check shows the direct-reference path translating
a guest byte written immediately before the append.

### Measured effect

A 60-second `aot-dbt` run with `REPIU_EXECUTION_TIME_PROFILE=1` reached its timeout normally with
zero malformed dispatch, an EEPROM SHA-256 matching the fixture, and only the pre-existing
`_GRHINTS@8` Glide-gap fatal. Normalized per translation, because this run performed 239 appends
against Task 328's 139: `arena_snapshot` fell from 404,524,860 to 7,970 ticks (-99.998%),
`placement` from 184,814,412 to 26,839,702 (-85.5%), `plan_build` from 81,728,912 to 26,907,556
(-67.1%), `image_emit` from 35,797,624 to 12,806,455 (-64.2%), `validate` from 3,089,728 to
772,181 (-75.0%), and the whole append from 710,135,523 to 67,367,429 ticks, a 10.5x reduction.
The translation unit stayed comparable at 1,058 instructions against 1,039, and snapshot bytes
went from 140,341,248 to 0.

Task 328's attribution caveat is confirmed: most of `placement` was the 133.8MB vector's
deallocation, putting the snapshot's whole lifecycle at roughly 79% of an append, near the top of
the 57-83% band predicted then — though part of that drop may be reduced memory pressure rather
than the free itself, so 79% is closer to an upper estimate. On the whole-run axis
`kAotDynamicTranslate` fell from 88.64% of the AOT transfer function axis to 26.44%, and
60-second progress was 62,566 against Task 328's 9,293 with a heartbeat of 784,320, though
run-to-run variance recorded in Tasks 325-328 means those multiples stay single-sample
observations.

### Unresolved

Why `plan_build`, `image_emit`, and `validate` also became 64-75% cheaper was not measured. The
likely explanation is that removing a 133.8MB allocation, zero-fill, and free per translation
removed the memory pressure that slowed every later phase, leaving decoding to read warm resident
pages, but this task did not measure it and the claim stays inferred. `plan_build` is now the
largest phase at 39.94% of an append and about 25,433 ticks per instruction, making it the next
candidate. This is a single sample, so the high-variance axes are not treated as settled.
