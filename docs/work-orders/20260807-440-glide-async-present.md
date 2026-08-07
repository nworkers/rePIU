# Task 440 작업 지시 — 비동기 present와 void 명령 큐 (opt-in)

설계: [20260807-440](../design/20260807-440-glide-async-present.md)

## 1. 범위

host 채널에 **유한 비동기 FIFO**를 더하고, `grBufferSwap`·`grBufferClear`·draw batch
flush를 그 큐로 보냅니다. `grBufferNumPending`은 **실제 미완료 swap 수**를 반환합니다.
`REPIU_GLIDE_ASYNC_PRESENT` opt-in이며 **기본값은 꺼짐**입니다.

**건드리지 않을 것:** 값 반환 게이트(LFB·질의·텍스처 다운로드)의 동기 rendezvous,
GL 명령의 제출 순서, draw batch의 flush 규칙(“draw 아닌 게이트 전에 flush”), 정점 디코드,
setter 생략 규칙.

## 2. 변경할 파일

| 파일 | 내용 |
|---|---|
| `glide_opengl_backend.h` · `.cpp` | 비동기 FIFO, `PostToHostThread`, pump의 선행 비우기, 미완료 swap 카운터, 실패 카운터, `BufferSwapAsync`/`BufferClearAsync`/`DrawPrimitiveBatchAsync` |
| `linexe_glide_boundary.cpp` | swap·clear 게이트와 flush 경로를 스위치에 따라 post로 분기, `grBufferNumPending`이 실제 값 반환 |
| `main.cpp` | 요약에 `async` 항목(게시 수·실패·최대 큐 길이·미완료 swap) |
| `glide_async_present_probe.{h,cpp}`(신규) · `main.cpp` · `CMakeLists.txt` | 순서·선행 비우기·역압·회계 단정 |
| `README.md` · 가이드 | 새 변수와 A/B 절차(**vsync ON 필수**) |

## 3. 구현 규칙

* **순서가 최우선입니다.** pump는 FIFO를 **전부 비운 뒤** 동기 슬롯을 실행합니다.
  post와 sync 사이에 순서를 뒤집을 수 있는 경로를 만들지 않습니다.
* **명령은 payload를 소유합니다.** flush 정점은 복사합니다.
* **상한은 두 겹입니다.** 미완료 swap 1개, FIFO 용량 유한. 가득 차면 post가 대기합니다.
* **teardown에서 교착하지 않습니다.** host가 더 이상 pump하지 않으면 post는 대기 대신
  즉시 실패로 집계하고, 종료 경로는 남은 FIFO를 비웁니다.
* **비동기 명령의 실패는 게이트를 거부하지 않습니다.** 원자 카운터로 집계해 요약에
  싣습니다(설계 §4).
* 스위치가 꺼져 있으면 **지금과 완전히 같은 경로**여야 합니다.

## 4. 검증

1. Win32 Release·Debug 빌드 통과.
2. probe 통과 — FIFO 순서, 동기 전 선행 비우기, 용량 역압, swap pending 회계, 실패 집계.
3. **vsync ON** 스모크 A/B — `=0`/`=1`에서 구현 공백 0 유지, `async failures` 0,
   ordinal 85 `gate/call`과 `glide-gate ÷ guest-run` 비교.
4. (사용자) gameplay A/B — 시각 회귀 확인 포함.

## 5. 완료 기준

1. `=0`이 지금과 동일하게 동작합니다.
2. `=1`에서 ordinal 85의 호출당 비용이 크게 내려가고 `async failures`가 0입니다.
3. `grBufferNumPending`이 실제 미완료 수를 반환합니다.
4. 화면이 같습니다.

---

# Task 440 Work Order — asynchronous present and a void-command queue, opt-in

## 1. Scope

Add a **bounded asynchronous FIFO** beside the host channel and post `grBufferSwap`,
`grBufferClear` and the draw-batch flush through it, with `grBufferNumPending` returning the
**real outstanding swap count**. Behind `REPIU_GLIDE_ASYNC_PRESENT`, **off by default**.

Not touched: the synchronous rendezvous for value-returning gates (LFB, queries, texture
downloads), GL submission order, the draw batch's flush rule, vertex decoding, and the setter
elision rules.

## 2. Files

The backend gains the FIFO, `PostToHostThread`, a pump that drains it before the synchronous slot,
outstanding-swap and failure counters, and async entry points for swap, clear and the batch flush;
the boundary routes those three through posts when the switch is on and answers
`grBufferNumPending` truthfully; the loader prints an `async` summary; a new probe with its CMake
and registration pins the rules; and the README and guide document the variable and the
**vsync-on** A/B.

## 3. Implementation rules

**Ordering comes first:** the pump drains the FIFO entirely before running the synchronous slot,
and no path may invert post against sync. **Commands own their payload** — flush vertices are
copied. **Two bounds**: one outstanding swap and a finite queue, with posts waiting when full.
**No teardown deadlock**: once the host stops pumping, a post is counted as a failure instead of
waiting, and the exit path drains what remains. **An async failure does not decline its gate**; it
is counted atomically and surfaced. With the switch off the path must be exactly today's.

## 4. Verification

Release and Debug builds; the probe covering ordering, drain-before-sync, back pressure, pending
accounting and failure counting; a **vsync-on** smoke comparing `=0` and `=1` with zero
implementation gaps and zero async failures, read through ordinal 85's per-call cost and the
`glide-gate` share; and the user's gameplay A/B including the visual check.

## 5. Done when

`=0` behaves exactly as today, `=1` drops ordinal 85's per-call cost sharply with zero async
failures, `grBufferNumPending` reports the real count, and the picture is unchanged.
