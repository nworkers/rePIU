# 20260803-404 AOT 세대 실패 원인 계측 작업 로그 / AOT Generation Failure Attribution Work Log

설계: [20260803-404](../design/20260803-404-aot-generation-failure-attribution.md)

작업 지시: [20260803-404](../work-orders/20260803-404-aot-generation-failure-attribution.md)

## 한국어

### 작업 요약

세대 실패 사유 계측을 추가했고 동작은 바꾸지 않았습니다. **목표였던 사유 문자열은
이번 5회 실행에서 재현되지 않아 확보하지 못했습니다.** 대신 같은 실행에서 **격리와
무관한 더 큰 비용**이 드러났습니다.

### 1. 구현

`ThreadContext`에 `generation_failure_trace`(용량 8, `target`/`page`/`quarantined`/
`terminal`/`message[96]`)와 count/overflow를 추가하고, `ResolveAotTransferTarget`의
`dynamic_translation_failed && retired_target` 분기에서 격리·terminal 결과가 확정된 뒤
한 항목을 기록합니다. `message`는 이미 손에 있던
`context->aot_translation_result.message`의 사본이며 새로 계산하는 값은 없습니다.
스냅샷과 요약 로그 2종을 추가했습니다. 상시 ON입니다.

### 2. 검증

* Release 빌드 성공. 기존 C4819 경고만 남았습니다.
* `repiu_aot_probe` 종료 코드 0, `selector_guard_all`을 포함한 `_all=` 항목 전부 true.
* 새 로그가 정상 출력됩니다: 5회 전부 `AOT generation failure events/overflow: 0/0`.
* pumpit1 45초 1회 **838 프레임**(변경 전 700~749). 회귀 없습니다.

### 3. 재현 실패 — 기록해 둡니다

pumpit3 45초 5회에서 격리가 **한 번도 발생하지 않았습니다**(`quarantines: 0` 5/5).
같은 날 오전 계측 전 10회에서는 6회 재현됐습니다. 소스가 같으므로 빌드 차이가 아니라
**실행별 비결정성**입니다. 계측은 상시 ON으로 남으므로 다음 발생 시 자동으로 잡힙니다.

| run | 프레임 | 격리 | 세대 실패 |
|---|---:|---:|---:|
| 1 | 1 | 0 | 0 |
| 2 | 87 | 0 | 0 |
| 3 | 2 | 0 | 0 |
| 4 | 150 | 0 | 0 |
| 5 | 102 | 0 | 0 |

### 4. 새로 확인된 것: 격리가 없어도 port I/O 예외가 지배합니다

렌더까지 간 3회(87/150/102 프레임)의 분해입니다.

| 항목 | run-02 | run-04 | run-05 |
|---|---:|---:|---:|
| wall (Gcyc) | 122.27 | 122.22 | 121.98 |
| port I/O 호출 | 857,750 | 1,074,586 | 990,793 |
| `0xC0000096`(privileged instruction) | 840,701 | 1,059,807 | 975,034 |
| 전체 예외 대비 | 90.4% | **92.9%** | 92.5% |
| 예외 없는 dispatch | 15,113 | 14,975 | 15,132 |
| **dispatch 적용률** | **1.8%** | **1.4%** | **1.5%** |
| VEH gap `other` | 51.27G (41.9%) | 60.30G (**49.3%**) | 57.74G (47.3%) |
| port I/O 핸들러 본체 | 6.74G (5.5%) | 7.32G (6.0%) | 7.30G (6.0%) |
| └ JAMMA scan | 5.99G | 6.48G | 6.45G |

**게스트 `IN` 한 번마다 CPU fault 한 번입니다.** `Port-I/O dispatch enabled: true`인데도
예외 없는 dispatch는 약 15,000회로 port I/O 호출의 **1.4~1.8%**에만 적용됩니다.

**주의 — gap은 순수 오버헤드가 아닙니다.** `VEH gap other`는 VEH 밖 시간이므로 커널
왕복에 더해 두 트랩 사이의 게스트 명령 실행이 포함됩니다. 지연 루프에서는 그 사이에
`cmp`/`jl`/`inc`/`sub` 네 개뿐이라 왕복이 지배하지만, 정확히는 상한입니다. gap 평균
56,338~60,248 cycle을 Task 347의 전이 가격 28,154~41,033과 비교하면 **왕복이 그 중
절반에서 3분의 2** 정도이며, 그래도 왕복만으로 wall의 약 30%입니다.

### 5. 이것이 기존 결론과 어떻게 이어지는가

Task 402가 "port I/O + 커널 왕복 46~56%"를 측정했고, Task 403은 그중 **본체(JAMMA
scan)만** 제거했습니다. **왕복은 손댄 적이 없고, 둘 중 큰 쪽입니다.** Task 403이
"프레임 개선을 확정하지 못한" 이유도 이것으로 설명됩니다.

### 6. 다음 대상

`kPortIo`는 planner에서 `EmitHleDispatchSlot`을 받게 되어 있고 그 함수는 항상 true를
돌려줍니다. 그런데 실측 적용률이 1.4%입니다. **왜 이 loop의 `IN`이 dispatch slot이
아니라 fault로 실행되는지**가 다음 질문이며, 여기에 wall의 30% 이상이 걸려 있습니다.

### 7. 후속 — 사유 문자열을 확보했습니다 (Task 405 측정 중)

Task 405 실행 4회 중 1회에서 격리가 발생해 계측이 잡았습니다.

```
Win32 AOT generation failure #1 target/page/quarantined/terminal/message:
  0x0301DFFE/0x0301D000/true/false/dynamic AOT entry was not active in the new image
```

**확인됨 세 가지입니다.**

1. **격리된 페이지는 `0x0301D000`** 입니다. Task 404가 재진입 거부 수와 port-I/O HLE
   횟수의 일치로 추론했던 페이지와 같습니다. 추론이 실측으로 확인됐습니다.
2. **사유는 여섯 후보 중 `dynamic AOT entry was not active in the new image`** 입니다.
   용량 고갈도, 번역기 실패도, coverage 부족도 아닙니다.
   `aot_code_cache_win32.cpp:1004`의 `entry_index == image.address_map.size()`이며,
   재번역이 이미지를 만들긴 했으나 **요청한 진입 주소에 대한 address-map 항목이 없었다**는
   뜻입니다.
3. **진입 주소 `0x0301DFFE`는 정상적인 명령 경계**입니다. 파일 offset `0x291FE`의
   `8a 2d 68 ec 34 00` = `mov ch,[0x34EC68]`이고, 직전 명령
   `09 15 64 ec 34 00`(6바이트)이 정확히 여기서 끝납니다. 잘못된 주소로 요청한 것이
   아닙니다.

따라서 다음 작업은 **재번역이 요청 진입 주소를 address map에 넣지 못하는 조건**입니다.
격리 정책 완화(재시도·유예)는 그 다음이며, 근인이 배치 결함이라면 재시도가 옳은
방향일 가능성이 높습니다.

### 8. 미확정

* 재번역이 요청 진입 주소를 map에 남기지 못하는 조건.
* dispatch 적용률이 1.4%인 이유 → **Task 405에서 해소**됐습니다(캐시가 아니라 arena
  실행).
* 격리 발생/미발생을 가르는 조건.

---

## English

### Summary

Added generation-failure instrumentation with no behaviour change. **The reason string this
task existed to capture did not reproduce in five runs**, so it is still unknown. The same
runs instead exposed a larger cost that has nothing to do with quarantine.

### 1. Implementation

`ThreadContext` gains `generation_failure_trace` (capacity 8, holding `target`, `page`,
`quarantined`, `terminal`, and `message[96]`) plus count and overflow counters. One entry is
recorded in the `dynamic_translation_failed && retired_target` branch of
`ResolveAotTransferTarget`, after the quarantine/terminal outcome is decided. The message is
a copy of `context->aot_translation_result.message`, already in hand; nothing new is
computed. The snapshot forwards the fields and two summary lines print them. Always on.

### 2. Verification

The Release build passed with only the pre-existing C4819 warnings; `repiu_aot_probe` exited
zero with every `_all=` check true; the new lines print correctly, reading
`AOT generation failure events/overflow: 0/0` in all five runs; and one 45-second pumpit1 run
rendered **838 frames** against 700-749 before the change, so no regression.

### 3. Reproduction failed, and that is recorded

Quarantine fired in **none** of five 45-second pumpit3 runs, against six of ten earlier the
same day on the same source. This is run-to-run nondeterminism rather than a build
difference. The instrumentation stays on, so the next occurrence is captured automatically.
Frames were 1, 87, 2, 150, and 102, with zero quarantines and zero generation failures in
every run.

### 4. New finding: port I/O exceptions dominate even without quarantine

Across the three runs that reached rendering:

| Metric | run-02 | run-04 | run-05 |
|---|---:|---:|---:|
| Wall (Gcyc) | 122.27 | 122.22 | 121.98 |
| Port I/O calls | 857,750 | 1,074,586 | 990,793 |
| `0xC0000096` privileged-instruction faults | 840,701 | 1,059,807 | 975,034 |
| Share of all exceptions | 90.4% | **92.9%** | 92.5% |
| Exception-free dispatches | 15,113 | 14,975 | 15,132 |
| **Dispatch coverage** | **1.8%** | **1.4%** | **1.5%** |
| VEH gap `other` | 51.27G (41.9%) | 60.30G (**49.3%**) | 57.74G (47.3%) |
| Port I/O handler body | 6.74G (5.5%) | 7.32G (6.0%) | 7.30G (6.0%) |
| of which JAMMA scan | 5.99G | 6.48G | 6.45G |

**Each guest `IN` costs one CPU fault.** Despite `Port-I/O dispatch enabled: true`, the
exception-free path covers only about 15,000 calls — 1.4-1.8% of them.

**Caveat: the gap is not pure overhead.** `VEH gap other` measures time outside the VEH, so
it includes the guest instructions between two traps as well as the kernel round trip. In
the delay loop only four cheap instructions separate consecutive traps, so the round trip
dominates, but the figure is an upper bound. Comparing the 56,338-60,248-cycle gap mean with
Task 347's 28,154-41,033-cycle transition price puts the round trip at roughly half to two
thirds of it — still about 30% of wall on its own.

### 5. How this connects to earlier conclusions

Task 402 measured "port I/O plus kernel round trip" at 46-56%, and Task 403 removed only the
**body** (the JAMMA scan). The round trip was never addressed and is the larger of the two,
which also explains why Task 403 could not establish a frame improvement.

### 6. Next target

The planner routes `kPortIo` to `EmitHleDispatchSlot`, which always returns true, yet
measured coverage is 1.4%. **Why this loop's `IN` executes as a fault instead of through a
dispatch slot** is the next question, and more than 30% of wall clock rests on it.

### 7. Follow-up: the reason string was captured (during Task 405's runs)

One of four Task 405 runs quarantined and the instrumentation caught it:

```
Win32 AOT generation failure #1 target/page/quarantined/terminal/message:
  0x0301DFFE/0x0301D000/true/false/dynamic AOT entry was not active in the new image
```

Three things follow. The quarantined page is **`0x0301D000`**, the same page Task 404
inferred from the exact match between rejected re-entries and the port-I/O HLE count, so the
inference is now measured. The reason is **`dynamic AOT entry was not active in the new
image`** — not capacity exhaustion, not translator failure, not missing coverage. That is
`entry_index == image.address_map.size()` at `aot_code_cache_win32.cpp:1004`, meaning the
re-translation did build an image but it carried **no address-map entry for the requested
entry address**. And the entry address `0x0301DFFE` is a legitimate instruction boundary:
file offset `0x291FE` holds `8a 2d 68 ec 34 00` (`mov ch,[0x34EC68]`), exactly where the
preceding six-byte `09 15 64 ec 34 00` ends, so nothing requested a misaligned address.

The next task is therefore **the condition under which a re-translation fails to map its own
requested entry address**. Softening the quarantine policy with retry or deferral comes after
that, and if the root cause is a placement defect, retry is likely the right direction.

### 8. Unresolved

The condition under which a re-translation omits its requested entry from the address map;
why dispatch coverage is 1.4%, which **Task 405 resolved** (the code runs in the arena, not
the cache); and what decides whether quarantine fires in a given run.
