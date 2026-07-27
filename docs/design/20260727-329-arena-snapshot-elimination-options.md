# 20260727-329 설계: arena 스냅샷 제거 방안 비교 / Design: Arena snapshot elimination options

## 한국어

### 1. 배경

Task 328은 `AppendWin32DynamicAotTranslation`이 진입 즉시 guest arena 전체
(`140,341,248`바이트, 약 133.8MB)를 zero-fill 후 복사하며, 그것이 append의 **56.96%**
임을 확인했습니다. 번역 1회는 평균 명령 1,039개를 다루고 7,830바이트를 emit하므로
**emit 대비 17,924배**를 복사합니다.

이 문서는 구현에 착수하기 전에 세 후보의 장단점을 고정합니다. 구현 방식 선택은
성능뿐 아니라 **정확성 계약**에 영향을 주므로 근거를 남겨야 합니다.

### 2. 선택에 영향을 주는 확인된 사실

착수 전에 코드로 확인한 사실입니다. 이후 판단은 전부 이 위에 놓입니다.

**(F1) 메모리 접근자는 포인터를 반환합니다.**
[`FindBytes`](../../src/runtime/aot_translation_plan.cpp)는 주소가
`[relocated_base_address, +memory.size())` 안이면 `object.memory.data() + offset`을
돌려주고, 밖이면 `nullptr`입니다.

**(F2) 범위 밖은 조용히 boundary가 됩니다.**
`FindBytes`가 `nullptr`이면 `++plan->outside_image_target_count` 후 해당 블록 또는
명령을 건너뜁니다. 즉 **범위를 줄이면 plan이 작아지고 번역되지 않은 boundary가
늘어납니다.** 성능이 아니라 실행 의미가 바뀝니다.

**(F3) 번역 중 guest thread는 차단되어 있습니다.**
Task 327이 확인했듯 `RequestAotDynamicTranslation`은 `WaitForSingleObject(INFINITE)`로
동기 대기합니다. 따라서 번역 도중 **guest는 arena를 수정할 수 없습니다.**

**(F4) write-watch 대상 guest 페이지는 항상 읽을 수 있습니다.**
`aot_page_coherence_win32.cpp`는 `PAGE_EXECUTE_READ`만 사용하며 `PAGE_NOACCESS`를
쓰지 않습니다. 즉 보호가 걸린 페이지도 직접 읽기가 가능합니다.

**(F5) arena는 precommit이고 전체가 읽힙니다.**
loader 로그는 `relocated image placed in precommitted Win32 process memory`이며,
현재 코드가 `bytes_read != runtime_size`를 실패로 처리하는데도 매번 성공합니다.
따라서 133.8MB 전 범위가 읽기 가능합니다.

**(F6) `RelocatedRuntimeObject`는 플랫폼 공용 구조체입니다.**
정적 로더 경로와 probe도 같은 구조를 사용하므로, 변경은 기존 사용처를 깨지 않는
**추가(additive)** 여야 합니다.

**(F7) 감지 신호가 모호합니다.**
`plan.outside_image_target_count`는 "arena 밖 주소"와 "내 창 밖 주소"를 구분하지
못합니다. 실제 게임 코드에도 arena 밖을 가리키는 target이 존재합니다.

### 3. 옵션 1 — 복사 제거 (live arena 직접 참조)

`RelocatedRuntimeObject`에 외부 메모리 포인터를 추가하고 `FindBytes`가 그것을
우선 사용하게 합니다. 복사와 zero-fill과 해제가 모두 사라집니다.

**장점**

* **스냅샷 비용이 완전히 소멸합니다.** append의 56.96%가 0에 수렴합니다.
* **해제 비용도 함께 사라집니다.** Task 328의 귀속 주의대로 `placement` 26.03%에는
  같은 133.8MB 버퍼의 해제가 포함되므로, 그 몫도 동시에 제거됩니다. 두 항목을 합치면
  append의 최대 83%가 대상입니다.
* **plan 결과가 현재와 동일합니다.** 보는 범위가 133.8MB로 같으므로 CFG 순회 결과가
  바이트 단위로 보존됩니다. (F2)의 위험이 발생하지 않습니다. 세 안 중 의미 보존이
  가장 강합니다.
* 구현 표면이 작습니다. (F6)을 지키는 additive 필드 하나와 `FindBytes` 분기 하나면
  됩니다.
* (F3)에 의해 번역 중 guest 쓰기가 없고, (F4)(F5)에 의해 읽기 실패나 AV 위험이
  낮습니다.

**단점 / 확인 필요**

* **스냅샷의 "시점 고정" 성질을 잃습니다.** 현재는 복사본이라 번역 도중 arena가
  바뀌어도 plan은 일관됩니다. 직접 참조는 그 보장을 구조적으로 잃습니다. (F3)에 의해
  guest 쓰기는 없지만, 이는 *현재 rendezvous가 동기라는 사실*에 의존하는 보장입니다.
  훗날 번역을 비동기로 바꾸면 이 안전성이 조용히 깨집니다. **그 의존성을 코드 주석과
  문서에 명시해야 합니다.**
* **다른 스레드의 arena 쓰기 여부가 미확인입니다.** SDL 메인 스레드, 오디오 스레드,
  poll thread가 arena 범위(`0x03000000`~`0x0B5D7000`)에 쓰는지 확인해야 합니다.
  Glide LFB와 LINEXE gate 코드가 arena 상단에 있으므로 특히 확인이 필요합니다.
  **이것이 옵션 1의 유일한 실질 리스크이며 착수 전 선행 조건입니다.**
* `ReadProcessMemory`는 읽을 수 없는 페이지에서 실패를 반환하지만, 직접 참조는 AV를
  냅니다. (F4)(F5)로 위험은 낮지만 fail-safe 성질은 약해집니다.

### 4. 옵션 2 — 페이지 단위 지연 복사

CFG가 실제로 건드린 페이지만 복사합니다.

**장점**

* 복사량이 실제 사용량에 비례합니다. 명령 1,039개면 수십 KB 수준입니다.
* **시점 고정 성질을 유지합니다.** 복사본이므로 옵션 1의 주요 단점이 없습니다.
  번역을 비동기로 바꾸더라도 안전성이 유지됩니다.
* 다른 스레드 쓰기 여부와 무관하게 안전합니다.

**단점**

* **공용 접근자 인터페이스를 바꿔야 합니다.** (F1)대로 `FindBytes`는 포인터를
  반환하므로, 지연 복사를 하려면 접근자가 캐시를 소유하고 상태를 갖도록 바꿔야
  합니다. 플랫폼 공용 코드의 추상화 변경이며 옵션 1보다 침습적입니다.
* x86 명령은 최대 15바이트이므로 **페이지 경계를 걸치는 접근** 처리가 필요합니다.
  `FindBytes`는 연속된 `bytes` 길이를 요구하므로 경계에서 두 페이지를 이어붙인
  임시 버퍼가 필요합니다.
* 이득은 옵션 1과 사실상 같은데 복잡도는 더 큽니다. 첫 접근마다 페이지 복사와 조회
  비용이 남습니다.

### 5. 옵션 3 — 고정 창 + 실패 시 확장

`guest_entry` 주변 N KB만 담은 object를 만들고, 부족하면 넓혀 재시도합니다.

**장점**

* 공용 구조체를 바꾸지 않아도 됩니다. `relocated_base_address`와 `virtual_size`,
  복사 범위만 줄이면 되므로 변경이 가장 국소적입니다.

**단점**

* **정확성 리스크가 있습니다.** (F2)대로 창 밖 주소는 조용히 boundary가 되어 plan이
  작아집니다. 창 크기를 잘못 잡으면 성능이 아니라 **실행 의미가 바뀝니다.** 다른 두
  안에는 이 성질이 없습니다.
* **확장 판단이 불가능에 가깝습니다.** (F7)대로 `outside_image_target_count`는 "창
  밖"과 "arena 밖"을 구분하지 못합니다. 실제 코드에 arena 밖 target이 있으므로
  "0이 아니면 확장" 규칙은 매번 확장으로 귀결될 수 있습니다. 정확한 판단을 하려면
  plan 생성기가 새 신호를 내보내야 하므로, 결국 **공용 코드를 바꿔야 합니다** —
  유일한 장점이 사라집니다.
* 확장할 때마다 plan을 처음부터 다시 생성하므로 최악의 경우 full build를 여러 번
  수행합니다.

### 6. 비교 요약

| 기준 | 옵션 1 직접 참조 | 옵션 2 지연 복사 | 옵션 3 고정 창 |
|---|---|---|---|
| 스냅샷 비용 제거 | 완전 | 거의 완전 | 부분 |
| 해제 비용 동시 제거 | 예 | 예 | 아니오 |
| plan 결과 보존 | **바이트 단위 동일** | 동일 | **달라질 수 있음** |
| 시점 고정 유지 | 아니오 (F3 의존) | 예 | 예 |
| 공용 코드 변경 | 작음 (additive) | 큼 (추상화) | 결국 필요 |
| 정확성 리스크 | 낮음 | 낮음 | **높음** |
| 선행 확인 | 타 스레드 arena 쓰기 | 없음 | 확장 판단 설계 |

### 7. 권고

**옵션 1을 권고하되, 착수 전에 "guest thread 외 다른 스레드가 arena에 쓰는가"를
확인하는 것을 선행 조건으로 둡니다.** 쓰기가 확인되면 옵션 2로 전환합니다.

**옵션 3은 제외를 권고합니다.** 유일한 장점인 "공용 코드 무변경"이 (F7)에 의해
성립하지 않으며, 정확성 리스크만 남습니다.

옵션 1 채택 시 반드시 남겨야 할 것:

* `FindBytes`가 외부 메모리를 볼 때의 **수명과 불변성 계약**을 구조체 주석에 명시.
* 이 안전성이 **동기 rendezvous에 의존**한다는 사실을 명시. 번역을 비동기로 바꾸려는
  후속 작업은 이 가정을 먼저 깨야 함을 경고로 남깁니다.
* A/B로 `arena_snapshot` 소멸과 `placement` 감소를 함께 확인하고, plan의
  `block_count`/`instruction_count`/`outside_image_target_count`가 변경 전후로
  동일한지 검증합니다. 이 세 값이 달라지면 의미가 보존되지 않은 것입니다.

### 8. 선행 조건 검증 결과 (Task 329 착수 시점)

7절이 요구한 선행 조건 — "guest thread 외 다른 스레드가 arena에 쓰는가" — 를 코드로
감사했습니다. **결과는 '쓰지 않는다'이며, 따라서 옵션 1을 그대로 채택합니다.**

프로세스에는 guest thread 외에 네 종류의 스레드가 있습니다.

| 스레드 | arena 쓰기 | 근거 |
|---|---|---|
| host main (poll loop) | **없음** | `PollThreadUntilExit`의 유일한 메모리 쓰기는 `WriteDosLowMemory(&context->dos_low_memory, 0x046C, ...)`이며 대상은 host 소유 `std::vector`인 `DosLowMemory::bytes`입니다. 나머지는 telemetry 구조체와 stderr입니다. |
| CD-DA (`cd_audio_wave_out`) | **없음** | CHD 섹터를 host `std::vector`로 읽어 `SDL_PutAudioStreamData`에 넘깁니다. |
| YMZ280B (`ymz280b_audio_out`) | **없음** | 샘플 ROM에서 host 블록 버퍼로 생성합니다. |
| 번역 워커 | **없음** | AOT cache(별도 `VirtualAlloc(nullptr, ...)`)에만 쓰고, arena에는 `VirtualProtect`로 보호만 바꿉니다. |

Glide backend는 별도 스레드가 아니라 **host main 스레드에서 guest 요청을 대행**합니다.
`InvokeOnHostThread`는 guest가 `host_command_complete_`를 기다리는 동안에만 명령을
실행하므로, host 스레드의 Glide 작업 구간은 guest가 gate 안에서 차단된 구간과 **정확히
겹칩니다.** guest thread는 한 번에 한 곳에서만 차단될 수 있으므로 번역 rendezvous와
Glide host command는 **상호 배타적**입니다. `PumpEvents`는 backend 상태만 만집니다.

`native_phase_sampler`는 guest thread를 `SuspendThread`하고 context만 읽습니다.
telemetry는 `runtime_base + 상수`에서 **읽기만** 합니다.

추가로 확인한 두 가지입니다.

* **decommit이 없습니다.** 저장소 전체에 `MEM_DECOMMIT`이 없고 `VirtualFree`는 모두
  `MEM_RELEASE`(종료 경로 또는 AOT cache)입니다. 즉 실행 중 arena 페이지가 사라지지
  않습니다.
* **`PAGE_NOACCESS`가 없습니다.** guest 페이지 보호는 `PAGE_EXECUTE_READ`,
  `PAGE_EXECUTE_READWRITE`, `PAGE_READWRITE` 사이에서만 바뀌므로 (F4) 그대로 항상
  읽을 수 있습니다.

**plan은 스냅샷 포인터를 보관하지 않습니다.** `AotInstructionRecord::bytes`가
`std::vector<std::uint8_t>` 복사본이므로 `BuildAotTranslationPlanFromEntry` 반환 후에는
arena를 가리키는 포인터가 남지 않습니다. 따라서 수명 계약은 plan build 구간으로
한정됩니다.

### 9. fail-safe 보완

옵션 1의 남은 단점은 "`ReadProcessMemory`는 실패를 반환하지만 직접 참조는 AV를 낸다"
입니다. 이를 **프로세스당 1회의 `VirtualQuery` 순회**로 보완합니다. 첫 동적 append에서
`[runtime_base, +runtime_size)`의 모든 region이 `MEM_COMMIT`이고 읽기 가능한 보호인지
확인하고, 실패하면 append를 거절합니다. 위의 "decommit 없음 / `PAGE_NOACCESS` 없음"이
성립하므로 1회 확인으로 충분하며, 매 append 비용은 0입니다.

---

## English

### 1. Background

Task 328 confirmed that `AppendWin32DynamicAotTranslation` zero-fills and copies the entire
133.8MB guest arena on entry, accounting for 56.96% of one append, while a translation covers
only 1,039 instructions and emits 7,830 bytes — 17,924 times less than it copies. This document
fixes the trade-offs before implementation, because the choice affects correctness contracts and
not only performance.

### 2. Established facts

**(F1)** `FindBytes` returns `object.memory.data() + offset` when the address falls inside
`[relocated_base_address, +memory.size())`, and `nullptr` otherwise. **(F2)** A `nullptr` result
increments `outside_image_target_count` and skips the block or instruction, so narrowing the
range silently shrinks the plan and adds untranslated boundaries — an execution-semantics
change, not a performance one. **(F3)** The guest thread is blocked in
`WaitForSingleObject(INFINITE)` for the whole translation (Task 327), so the guest cannot
modify the arena meanwhile. **(F4)** Write-watched guest pages use `PAGE_EXECUTE_READ` and never
`PAGE_NOACCESS`, so they stay directly readable. **(F5)** The arena is precommitted and the
current full-range `ReadProcessMemory` succeeds every time, proving the whole 133.8MB is
readable. **(F6)** `RelocatedRuntimeObject` is platform-neutral and shared with the static
loader path, so changes must be additive. **(F7)** `outside_image_target_count` cannot
distinguish "outside the arena" from "outside my window", and real code does target outside the
arena.

### 3. Option 1 — remove the copy, reference the live arena

Adding an external memory pointer to `RelocatedRuntimeObject` and preferring it in `FindBytes`
eliminates the copy, the zero-fill, and the deallocation.

It removes the snapshot cost entirely and, per the Task 328 attribution caveat, also removes the
same buffer's deallocation counted inside `placement`, so together up to 83% of an append is in
scope. Because the visible range stays the same 133.8MB, the plan is preserved byte for byte and
(F2) cannot bite; this is the strongest semantic preservation of the three. The implementation
surface is small: one additive field respecting (F6) and one branch in `FindBytes`. (F3) means
no guest writes during translation, and (F4) and (F5) make read failure or access violations
unlikely.

The cost is losing the snapshot's point-in-time fixity, which currently makes the plan
self-consistent even if the arena changed mid-translation. (F3) covers this today, but the
guarantee then depends on the rendezvous being synchronous, and a future move to asynchronous
translation would silently break it, so the dependency must be recorded in code and
documentation. The one genuine open risk is whether threads other than the guest — SDL main,
audio, poll — write into the arena range, which matters because the Glide LFB and LINEXE gate
code live near its top; confirming this is a prerequisite. Direct references also lose the
fail-safe property of `ReadProcessMemory`, which reports failure where a raw read would fault.

### 4. Option 2 — lazy per-page copy

Copying only pages the CFG touches makes the copy proportional to actual use, tens of kilobytes
for 1,039 instructions, while keeping the point-in-time fixity that Option 1 gives up and
staying safe regardless of other threads.

Against that, (F1) means the accessor returns a raw pointer, so lazy copying requires the
accessor to own a cache and carry state — an abstraction change to shared platform-neutral code
and more invasive than Option 1. Instructions up to 15 bytes can straddle page boundaries, so
the accessor needs a stitched temporary buffer for contiguity. The benefit is essentially the
same as Option 1 for materially more complexity, and per-page copy and lookup costs remain.

### 5. Option 3 — fixed window, widen on failure

Its only advantage is needing no shared-structure change, since only the base, size, and copied
range shrink.

The drawbacks are decisive. Per (F2) addresses outside the window silently become boundaries and
the plan shrinks, so a badly chosen window changes execution semantics rather than performance —
a property the other two options do not have. Per (F7) the widening decision is barely possible,
because `outside_image_target_count` conflates window misses with genuine external targets, and
since real code targets outside the arena, "widen when nonzero" may widen every time. Making the
decision precise requires a new signal from the plan builder, which means changing shared code
after all and removing the option's only advantage. Each widening also rebuilds the plan from
scratch, so the worst case performs several full builds.

### 6. Recommendation

Option 1, conditional on first confirming that no thread other than the guest writes into the
arena; if such writes exist, switch to Option 2. Option 3 should be dropped: its only advantage
does not hold under (F7), leaving only correctness risk.

Adopting Option 1 requires documenting the lifetime and immutability contract for externally
referenced memory in the structure comment, recording explicitly that this safety depends on the
synchronous rendezvous so any future asynchronous translation must break that assumption first,
and verifying in the A/B that the plan's `block_count`, `instruction_count`, and
`outside_image_target_count` are unchanged — divergence in those three means semantics were not
preserved.

### 7. Prerequisite audit result

The prerequisite was audited in code and the answer is that no thread other than the guest
writes into the arena, so Option 1 is adopted unchanged. The host main poll loop's only memory
write is `WriteDosLowMemory` into the host-owned `DosLowMemory::bytes` vector, with everything
else going to telemetry structures and stderr; both audio threads work exclusively in host
buffers fed from a CHD image and a sample ROM; and the translation worker writes only the AOT
cache, a separate `VirtualAlloc(nullptr, ...)` region, changing arena pages by `VirtualProtect`
alone. The Glide backend is not a separate thread but work performed on the host main thread on
the guest's behalf: `InvokeOnHostThread` runs a command only while the guest waits for
`host_command_complete_`, so a Glide host command and a translation rendezvous are mutually
exclusive, since the guest thread can block in only one place at a time. `PumpEvents` touches
backend state only, the native phase sampler suspends the guest and reads its context, and
telemetry only reads `runtime_base + constant`.

Two further facts came out of the audit. Nothing decommits: the repository contains no
`MEM_DECOMMIT`, and every `VirtualFree` is a `MEM_RELEASE` on a teardown path or on the AOT
cache. Nothing becomes unreadable: guest page protection moves only among `PAGE_EXECUTE_READ`,
`PAGE_EXECUTE_READWRITE`, and `PAGE_READWRITE`, which keeps (F4) true for the whole run. The
plan also keeps no pointer into the source, because `AotInstructionRecord::bytes` is an owning
vector, so the lifetime contract is bounded by the plan build itself.

### 8. Restoring the fail-safe

Option 1's remaining drawback is that `ReadProcessMemory` reports failure where a direct
reference faults. A single `VirtualQuery` walk per process, performed on the first dynamic
append, confirms every region in `[runtime_base, +runtime_size)` is committed and readable and
refuses the append otherwise. Because nothing decommits and nothing becomes `PAGE_NOACCESS`,
one verification is sufficient and the per-append cost is zero.
