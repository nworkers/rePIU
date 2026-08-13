# AOT residency 표본화 게이트 작업 로그

## 요약

`pumpit8` cycle 프로파일에서 `guest-run`의 **13.67%** 를 쓰던 진단 계측
`AccumulateAotResidency`에 opt-in 게이트를 달고 기본값을 OFF로 바꿨습니다. 표본기는
`src/platform/win32/telemetry/aot_residency_sample.{h,cpp}`로 분리했고, 호출마다
반복되던 `ZydisDecoderInit`을 1회 초기화로 승격했습니다.

관련: [설계](../design/20260813-478-aot-residency-sampling-gate.md),
[작업 지시](../work-orders/20260813-478-aot-residency-sampling-gate.md),
[current-execution-frontier](../analysis/current-execution-frontier.md)

## 어떻게 찾았는가

사용자가 `pumpit8` 성능 저하를 보고했고, 처음에는 실행 로그의 빈도 지표만으로
inline cache 패치를 1순위로 지목했습니다. 그 판정은 유지됐지만 **두 가지가
틀렸습니다.**

1. residency 계측을 "부차 요인"으로 낮춰 잡았는데 실측 **13.67%** 였습니다.
2. IC 패치 단가를 `VirtualProtect`/`FlushInstructionCache` syscall 탓으로 짚었는데,
   실제 지배 요인은 **8,019개 site를 도는 선형 탐색**이었습니다.

둘 다 `REPIU_EXECUTION_TIME_PROFILE=1` 실행 한 번으로 갈렸습니다. **빈도만으로
단가를 추정하지 말 것** — 이번에 두 번 다 빗나갔습니다.

## 측정 (2026-08-13, vsync OFF)

wall 117초, `guest-run` 314,692,501,094 cycle, buffer swap 691회 = 약 8.1 fps.

| 항목 | 값 |
|---|---:|
| `kAotResidency` cycles | 43,004,277,414 |
| `guest-run` 대비 | **13.67%** |
| 호출 수 | 3,076,235 |
| 호출당 | 13,979 cycle (약 3.8 µs) |

호출당 13,979 cycle은 `ZydisDecoderDecodeFull`이 표본당 평균 약 28회 도는 값입니다.
상한 64에 자주 닿고 있었습니다.

## 한 일

| # | 변경 | 파일 |
|---|---|---|
| 1 | 표본기 분리 (본문 이동, 이름·서명 유지) | `telemetry/aot_residency_sample.{h,cpp}` (신규) |
| 2 | `REPIU_AOT_RESIDENCY_SAMPLE` opt-in 게이트, 기본 OFF | 같은 파일 |
| 3 | 게이트 검사를 `ExecutionTimeScope`·decoder 초기화보다 앞에 배치 | 같은 파일 |
| 4 | `ZydisDecoderInit`을 함수 지역 `static` 1회 초기화로 승격 | 같은 파일 |
| 5 | 본문·선언 제거 | `aot/aot_runtime_dispatch.{h,cpp}` |
| 6 | 호출 지점 9곳 include 갱신 | `aot_dbt_dispatch.cpp`, `aot_dbt_hle_dispatch.cpp`, `aot_dbt_glide_gate_dispatch.cpp`, `aot_runtime_dispatch.cpp` |
| 7 | 요약 줄에 게이트 상태 추가 | `host/win32/main.cpp` |
| 8 | 소스 등록 | `CMakeLists.txt` |

header는 `thread_context.h`를 include하지 않고 `struct ThreadContext;`를
전방 선언합니다. `repiu` 호스트 타깃은 `repiu_exe`의 PRIVATE include 디렉터리를
물려받지 않으므로, include했다면 `main.cpp`에서 해석되지 않습니다. 같은 이유로
`aot_dbt_glide_gate_dispatch.h`가 이미 쓰던 방식입니다.

## 검증

| 항목 | 결과 |
|---|---|
| Win32 x86 **Release** 빌드 | 통과 (기존 C4819 코드페이지 경고, LNK4217만) |
| Win32 x86 **Debug** 빌드 | 통과 (exit 0) |
| `aot_probe` (pumpit8 이미지) | **exit 0**, `inline_cache_all=true` 포함 전 항목 통과 |
| `ResolveOptInToggle` 기본값 | `nullptr`·빈 문자열 모두 `false` — 기본 OFF 확인 |

### 사용자 구동 결과 (2026-08-13 20:58, vsync OFF)

**확인됨 — 게이트는 의도대로 동작합니다.**

```
Win32 AOT residency enabled/total/samples/avg/max/coverage%: false/0/0/0.00/0/0.00
Win32 aot transfer function cycles ...residency: 0
Win32 aot transfer function count  ...residency: 0
```

기본값이 OFF이고, `kAotResidency`가 **43,004,277,414 cycle / 3,076,235회 → 0 / 0**
으로 완전히 사라졌습니다. 이 항목은 비교가 아니라 **구조적 사실**이므로 장면과
무관하게 성립합니다. 실행은 정상 종료했고 요약도 온전히 남았습니다.

**미확정 — fps 이득은 이 한 쌍으로 판정할 수 없습니다.**

| 지표 | 이전 (20:06) | 이후 (20:58) |
|---|---:|---:|
| `guest-run` cycles | 314,692,501,094 | 109,770,096,849 |
| buffer swap | 691 | 346 |
| **패치/프레임** | **1,742** | **3,065 (+76%)** |
| **평균 draw 배치** | **15.98** | **7.69 (−52%)** |
| 삼각형/프레임 | 52.8 | 51.1 |
| `kAotReturn` | 46.75% | 44.86% |
| glide-gate | 10.35% | 7.21% |
| `unaccounted` | 75.96% | 77.30% |

프레임당 작업량이 3% 이내로 일치해야 한다는 조건을 **패치/프레임이 +76%,
평균 배치가 −52%로 크게 벗어납니다.** 교차 검증 두 지표도 방향이 반대입니다.

| 교차 지표 | 변화 |
|---|---:|
| cycle당 swap | **+43.6%** |
| cycle당 primitive | **−41.8%** |

2026-08-07 세션 규칙은 fps·cycle당 swap·cycle당 primitive **셋이 서로 검증할 때만**
프레임을 근거로 쓸 수 있다고 정합니다. 여기서는 둘이 반대 방향이므로 **두 실행은
다른 장면**이고, 겉보기 fps 8.10 → 11.64는 근거로 쓰지 않습니다.

**따라서 이번 작업의 판정은 "13.67%를 제거했다"까지이고, "fps가 얼마나 올랐다"는
아직 측정되지 않았습니다.** 같은 구간을 재현하는 A/B는 Task 479 이후로 미룹니다 —
479가 같은 return 경로를 건드리므로 그때 한 번에 재는 편이 실행 횟수를 아낍니다.

`REPIU_AOT_RESIDENCY_SAMPLE=1` 되켜기 확인은 아직 수행하지 않았습니다.

## 회고

* **게이트 없는 진단 계측이 기본 경로에 남아 있었습니다.** `ExecutionTimeScope`가
  null 검사를 하니 "프로파일을 꺼두면 공짜"라고 읽기 쉬운 모양이었는데, 정작 비싼
  Zydis 루프에는 아무 분기가 없었습니다. 계측을 추가할 때는 **측정 스코프가 아니라
  측정 대상 작업**에 게이트를 다는지 확인해야 합니다.
* **다음 작업(Task 479, IC 패치 site 조회 인덱스화)은 이미 있는 해법의 재적용입니다.**
  Task 334가 `FindAotGuestAddress`의 동일한 선형 탐색을 인덱스로 해결했는데 패치
  경로에는 적용되지 않았습니다. 같은 결함이 두 곳에 있었고 한 곳만 고쳐졌던 셈입니다.

---

# AOT Residency Sampling Gate Work Log

## Summary

Added an opt-in gate, defaulting to off, to `AccumulateAotResidency` — diagnostic
instrumentation that a `pumpit8` cycle profile measured at **13.67% of
`guest-run`**. The sampler moved to
`src/platform/win32/telemetry/aot_residency_sample.{h,cpp}`, and the per-call
`ZydisDecoderInit` was promoted to a one-time initialization.

## How it was found

The user reported the `pumpit8` slowdown, and the first pass ranked inline-cache
patching first from run-log frequencies alone. That ranking held, but **two
things in it were wrong**: the residency sampler was dismissed as secondary when
it measures **13.67%**, and the patch's unit price was attributed to the
`VirtualProtect`/`FlushInstructionCache` syscalls when the real driver is a
**linear scan over 8,019 sites**. One run with
`REPIU_EXECUTION_TIME_PROFILE=1` settled both. **Do not infer unit price from
frequency** — it missed twice here.

## Measurement (2026-08-13, vsync off)

117 s wall, `guest-run` 314,692,501,094 cycles, 691 buffer swaps ≈ 8.1 fps.

| Item | Value |
|---|---:|
| `kAotResidency` cycles | 43,004,277,414 |
| Share of `guest-run` | **13.67%** |
| Calls | 3,076,235 |
| Per call | 13,979 cycles (≈3.8 µs) |

That per-call figure corresponds to roughly 28 `ZydisDecoderDecodeFull` calls per
sample, so the cap of 64 was frequently approached.

## What changed

The sampler was extracted to its own telemetry file with the name and signature
kept; the gate (`REPIU_AOT_RESIDENCY_SAMPLE`, resolved by `ResolveOptInToggle`)
is checked ahead of both the `ExecutionTimeScope` and the decoder; the decoder is
a function-local `static`; the body and declaration left
`aot_runtime_dispatch.{h,cpp}`; the nine call sites took the new include; the
`main.cpp` summary line now carries the gate state; and `CMakeLists.txt`
registers the source.

The header forward-declares `struct ThreadContext;` instead of including
`thread_context.h`, because the `repiu` host target does not inherit
`repiu_exe`'s private include directories — the same reason
`aot_dbt_glide_gate_dispatch.h` already does this.

## Verification

| Item | Result |
|---|---|
| Win32 x86 **Release** build | Passed (only pre-existing C4819 and LNK4217 warnings) |
| Win32 x86 **Debug** build | Passed (exit 0) |
| `aot_probe` (pumpit8 image) | **exit 0**, all items including `inline_cache_all=true` |
| `ResolveOptInToggle` default | `false` for both null and empty — gate is off by default |

### User run (2026-08-13 20:58, vsync off)

**Confirmed — the gate behaves as designed.** The default run reports
`Win32 AOT residency enabled/total/samples/avg/max/coverage%:
false/0/0/0.00/0/0.00`, and `kAotResidency` went from **43,004,277,414 cycles
over 3,076,235 calls to 0 / 0**. That is a structural fact rather than a
comparison, so it holds independently of the scene. The run exited cleanly with a
complete summary.

**Not established — the fps gain cannot be judged from this pair.**

| Metric | Before (20:06) | After (20:58) |
|---|---:|---:|
| `guest-run` cycles | 314,692,501,094 | 109,770,096,849 |
| Buffer swaps | 691 | 346 |
| **Patches per frame** | **1,742** | **3,065 (+76%)** |
| **Mean draw batch** | **15.98** | **7.69 (−52%)** |
| Triangles per frame | 52.8 | 51.1 |
| `kAotReturn` | 46.75% | 44.86% |
| glide-gate | 10.35% | 7.21% |
| `unaccounted` | 75.96% | 77.30% |

Per-frame work had to agree within 3% and misses badly — patches per frame
+76%, mean batch −52% — and the two cross-checks point in opposite directions:
swaps per cycle **+43.6%** against primitives per cycle **−41.8%**. The
2026-08-07 rule admits frames only when fps, swaps per cycle, and primitives per
cycle all corroborate each other, so **these are different scenes** and the
apparent 8.10 → 11.64 fps is not used as evidence.

**This task's verdict therefore stops at "13.67% removed"; "fps rose by X" is
still unmeasured.** The matched-scene A/B is deferred until after Task 479, which
touches the same return path, so both can be measured in one run.

Re-enabling with `REPIU_AOT_RESIDENCY_SAMPLE=1` has not been checked yet.

## Retrospective

* **Ungated diagnostic instrumentation sat on the default path.** The
  `ExecutionTimeScope` null check made it look free with profiling off, but no
  branch guarded the expensive Zydis loop. When adding instrumentation, check
  that the gate covers **the measured work, not just the measuring scope**.
* **The next task (479, indexing the patch site lookup) reapplies an existing
  fix.** Task 334 solved the identical linear scan in `FindAotGuestAddress` with
  an index and the patch path never got it — the same defect existed in two
  places and only one was repaired.
