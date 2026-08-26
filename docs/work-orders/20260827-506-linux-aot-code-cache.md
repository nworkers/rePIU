# Task 506 작업 지시 — Linux AOT 코드 캐시

설계: [20260827-506](../design/20260827-506-linux-aot-code-cache.md) ·
작업 로그: 20260827-506 (구현 시 작성)

## 0. 시작 전에 — 규모를 다시 세십시오

설계가 센 숫자는 이렇습니다. **먼저 재현하고 시작하십시오.** 숫자가 다르면 코드가 움직인
것이고, 지시서가 낡은 것입니다.

```bash
grep -oE "\b(VirtualAlloc|VirtualFree|VirtualProtect|VirtualQuery|GetSystemInfo|FlushInstructionCache|GetCurrentProcess|GetLastError)\b" \
  src/platform/win32/aot_code_cache_win32.cpp | sort | uniq -c
```

기대값: `VirtualProtect` 17, `VirtualFree` 10, `FlushInstructionCache`·`GetCurrentProcess` 각 6,
`GetLastError` 4, `VirtualAlloc` 2, `VirtualQuery`·`GetSystemInfo` 각 1.
`aot_page_coherence_win32.cpp`는 `VirtualProtect` 8, 나머지 소수.

## 1. 명령 캐시 플러시 계층을 먼저 만드십시오

이것이 **새로 만들어야 하는 유일한 것**입니다. 나머지는 3b가 이미 덮습니다.

`include/repiu/platform/instruction_cache.h`에 하나:

```cpp
void FlushInstructionCache(void* address, std::size_t bytes);
```

* Windows: `::FlushInstructionCache(GetCurrentProcess(), address, bytes)`.
* Linux: `__builtin___clear_cache`.

**헤더에 x86에서 이것이 아무것도 하지 않는다는 사실을 적으십시오.** x86의 명령 캐시는 데이터
캐시와 일관되므로 아키텍처상 불필요합니다. 그리고 이것이 frontier 8절이 모으는
*"컴파일되면서 아무것도 안 하는 코드"*와 **의도가 반대**라는 점을 함께 적으십시오 — 저것들은
실수로 죽어 있었고, 이것은 **알고서 비워 둔 자리**입니다. 그 구분이 헤더에 없으면 다음 사람이
8절의 목록에 이것을 올립니다.

호출부를 지우지 마십시오. 코드를 쓰고 실행 전에 플러시하는 것은 이식 가능한 코드의 규약이며,
호출을 없애면 "왜 없는지"가 함께 사라집니다.

## 2. 나머지는 3b 호출로 바꿉니다

| Win32 | 대체 |
|---|---|
| `VirtualAlloc` (예약/커밋) | `ReserveMemory` / `CommitMemory` |
| `VirtualProtect` | `ProtectMemory` — **`previous`를 쓰는 호출부를 확인할 것** |
| `VirtualFree` | `ReleaseMemory` |
| `VirtualQuery` | `QueryMemory` / `IsRangeReadable` |
| `GetSystemInfo`(페이지 크기) | 3b가 이미 답하는지 먼저 볼 것, 없으면 `sysconf(_SC_PAGESIZE)` |
| `GetLastError` | `errno` — **진단용이지 제어 흐름용이 아닙니다** |

**`ProtectMemory`의 `previous`를 흘리지 마십시오.** 3b 헤더가 경고합니다 — `mprotect`는
이전 보호를 알려주지 않으므로 계층이 따로 추적합니다. 동적 번역 경로는 캐시를 쓰기 가능으로
바꾸고, 패치하고, 실행 가능으로 되돌리는 주기를 반복하므로 **여기가 그 값을 실제로 쓰는
곳입니다.**

`VirtualQuery` 두 곳이 묻는 것은 "commit되어 있고 읽을 수 있는가"뿐입니다. 3b 헤더가
"VirtualQuery에는 값싼 대응물이 없다"고 적은 것은 일반형에 대한 말이니, **그 문장에 막혀
되돌리지 마십시오** — 3d-19가 여기서 한 번 되돌아갔습니다.

## 3. `requires Win32` 넷을 지우십시오

```
aot_code_cache_win32.cpp:812   AOT 코드 캐시 배치
aot_code_cache_win32.cpp:1023  동적 번역
aot_code_cache_win32.cpp:1775  인라인 캐시 패치
aot_page_coherence_win32.cpp:637  게스트 페이지 회수
```

넷 다 **컴파일되면서 아무것도 안 하는** 함수입니다(frontier 8절의 목록). 작업이 끝나면
`grep -rn "requires Win32" src/`가 **아무것도 찾지 못해야 합니다.**

## 4. 검증

### Windows 무영향은 증명하십시오

주장하지 마십시오. 505가 쓴 방법이 값싸고 결정적입니다 — **원본에서 "Windows가 보던 코드"를
재구성해 현재 파일과 대조**하십시오. 다만 506은 505와 달리 호출 자체를 바꾸므로 텍스트가
같을 수 없습니다. 그러니 여기서는 **동작으로** 증명합니다.

| 대상 | 기준 |
|---|---|
| Windows Debug 빌드 | 오류 없음 |
| Windows `repiu_core_probe` | `core_probe_total=15 failures=0` |
| Windows `repiu_aot_probe` 전체 | exit 0, romset-config 94/0, nvram-path 14/0 |
| **Windows pumpit1 (dynamic)** | **렌더가 이전과 같음 — 이것이 진짜 회귀 시험입니다** |

`repiu_aot_probe`가 특히 중요합니다. AOT 캐시 방출·패치를 직접 단정하므로, 이 이식이 Windows
동작을 바꾸면 여기서 먼저 드러납니다.

### Linux

| 대상 | 기준 |
|---|---|
| Linux i386 빌드 | `repiu` 링크까지 성공 |
| `grep -rn "requires Win32" src/` | **결과 없음** |
| Linux `repiu_core_probe` | `core_probe_total=15 failures=0` |
| Linux DOS/4GW 샘플 | 3d-19 기준선 유지 (exit 2, 초점 오프셋 0x10, opcode 0x80) |
| **Linux pumpit1 (`dynamic`)** | **버퍼 스왑이 0을 벗어남** |

실행 시 유의(505에서 확립):

* **저장소 루트에서** 실행합니다. 로더가 `roms`·`build/runtime_mounts`를 상대 경로로 찾습니다.
* `REPIU_STALL_TIMEOUT_MS=0`으로 무진행 감시견을 끕니다.
* `REPIU_GLIDE_PIXEL_DIAG=1`이 **실행 중에** 스왑과 non-black 픽셀을 찍습니다. teardown
  SIGTRAP(exit 133)이 Glide 요약을 잘라먹으므로 이쪽을 쓰십시오.
* **관측 창을 넉넉히 잡으십시오.** 505가 30초로 "느리다", 240초로 "갇혔다"는 **틀린 결론**을
  두 번 냈고 1,200초에서야 실상이 보였습니다.

## 5. 완료 조건

**Linux pumpit1의 버퍼 스왑이 0을 벗어납니다.**

빌드도 링크도 `dynamic` 백엔드가 도는 것도 완료 조건이 아닙니다. 505가 배운 것이 그것입니다 —
**성공 신호 하나로 성공을 판정하지 말 것.** 505의 `opened=1`이 더미 폴백으로도 나왔던 것처럼,
`dynamic`이 "돈다"는 것도 아무것도 그리지 않으면서 참일 수 있습니다.

**무엇이 그려지는지의 정확성은 이 단계의 완료 조건이 아닙니다.** 첫 픽셀과 맞는 그림은 다른
질문이고, 둘을 묶으면 어느 쪽이 실패했는지 말할 수 없게 됩니다.

---

# Task 506 Work Order — The Linux AOT code cache

Design: [20260827-506](../design/20260827-506-linux-aot-code-cache.md) ·
Work log: 20260827-506 (to be written during implementation)

## 0. Before starting — count the size again

These are the design's numbers. **Reproduce them first.** If they differ, the code has moved and this
order is stale.

```bash
grep -oE "\b(VirtualAlloc|VirtualFree|VirtualProtect|VirtualQuery|GetSystemInfo|FlushInstructionCache|GetCurrentProcess|GetLastError)\b" \
  src/platform/win32/aot_code_cache_win32.cpp | sort | uniq -c
```

Expected: `VirtualProtect` 17, `VirtualFree` 10, `FlushInstructionCache` and `GetCurrentProcess` 6
each, `GetLastError` 4, `VirtualAlloc` 2, `VirtualQuery` and `GetSystemInfo` 1 each.
`aot_page_coherence_win32.cpp` has `VirtualProtect` 8 and a few others.

## 1. Build the instruction-cache flush layer first

This is the **only thing that has to be built**. 3b already covers the rest.

One function in `include/repiu/platform/instruction_cache.h`:

```cpp
void FlushInstructionCache(void* address, std::size_t bytes);
```

* Windows: `::FlushInstructionCache(GetCurrentProcess(), address, bytes)`.
* Linux: `__builtin___clear_cache`.

**Write into the header that this does nothing on x86.** x86's instruction cache is coherent with its
data cache, so the call is architecturally unnecessary. And write that this is the **opposite in
intent** to what frontier section 8 collects — those functions were dead by mistake; this one is
**deliberately empty**. Without that distinction in the header, the next reader adds it to section
8's list.

Do not delete the call sites. Flushing after writing code and before running it is the contract
portable code keeps, and removing the calls removes the reason with them.

## 2. Move the rest onto 3b

| Win32 | Replacement |
|---|---|
| `VirtualAlloc` (reserve/commit) | `ReserveMemory` / `CommitMemory` |
| `VirtualProtect` | `ProtectMemory` — **check which call sites use `previous`** |
| `VirtualFree` | `ReleaseMemory` |
| `VirtualQuery` | `QueryMemory` / `IsRangeReadable` |
| `GetSystemInfo` (page size) | see whether 3b already answers it; otherwise `sysconf(_SC_PAGESIZE)` |
| `GetLastError` | `errno` — **for the record, not for control flow** |

**Do not drop `ProtectMemory`'s `previous`.** The 3b header warns about it: `mprotect` does not report
the protection it replaced, so the layer tracks it separately. The dynamic translation path cycles the
cache between writable and executable around every patch, which makes **this the place that actually
consumes that value.**

The two `VirtualQuery` sites ask only "is it committed and readable". The 3b header's remark that
VirtualQuery "has no cheap Linux counterpart" is about the general form, so **do not turn back at that
sentence** — 3d-19 turned back here once already.

## 3. Delete the four `requires Win32` returns

```
aot_code_cache_win32.cpp:812   AOT code cache placement
aot_code_cache_win32.cpp:1023  dynamic translation
aot_code_cache_win32.cpp:1775  inline-cache patching
aot_page_coherence_win32.cpp:637  guest page retirement
```

All four are functions that **compile and do nothing** — section 8's list. When this work is done,
`grep -rn "requires Win32" src/` must find **nothing**.

## 4. Verification

### Prove Windows is unaffected

Do not assert it. 505's method is cheap and decisive — **reconstruct the code Windows saw and compare
it against the file now.** But 506, unlike 505, changes the calls themselves, so the text cannot
match. Here it is proven **by behaviour** instead.

| Target | Criterion |
|---|---|
| Windows Debug build | no errors |
| Windows `repiu_core_probe` | `core_probe_total=15 failures=0` |
| Windows `repiu_aot_probe`, full pass | exit 0, romset-config 94/0, nvram-path 14/0 |
| **Windows pumpit1 (dynamic)** | **renders as before — this is the real regression test** |

`repiu_aot_probe` matters most: it asserts cache emission and patching directly, so if this port
changes Windows behaviour it shows there first.

### Linux

| Target | Criterion |
|---|---|
| Linux i386 build | links as far as `repiu` |
| `grep -rn "requires Win32" src/` | **no results** |
| Linux `repiu_core_probe` | `core_probe_total=15 failures=0` |
| The Linux DOS/4GW sample | 3d-19's baseline holds (exit 2, focus offset 0x10, opcode 0x80) |
| **Linux pumpit1 (`dynamic`)** | **buffer swaps leave zero** |

When running (established in 505):

* Run **from the repository root**; the loader resolves `roms` and `build/runtime_mounts` relatively.
* `REPIU_STALL_TIMEOUT_MS=0` to disable the no-progress watchdog.
* `REPIU_GLIDE_PIXEL_DIAG=1` prints swaps and non-black pixels **during** the run. The teardown
  SIGTRAP (exit 133) truncates the Glide summary, so use this instead.
* **Give the observation a long window.** 505 drew **two wrong conclusions** — "slow" at 30 seconds,
  "trapped" at 240 — and only 1,200 seconds showed what was happening.

## 5. Completion criteria

**Buffer swaps leave zero on Linux pumpit1.**

Not the build, not the link, not the `dynamic` backend running. That is 505's lesson: **do not judge
success from a single success signal.** Just as 505's `opened=1` was also what the dummy fallback
returned, "dynamic runs" can be true while nothing is drawn.

**Whether the picture is correct is not a criterion here.** A first pixel and a right picture are
different questions, and tying them together makes it impossible to say which one failed.
