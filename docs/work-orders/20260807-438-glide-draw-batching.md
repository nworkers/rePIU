# Task 438 작업 지시 — Glide draw batching (opt-in)

설계: [20260807-438](../design/20260807-438-glide-draw-batching.md)

## 1. 범위

`grDrawTriangle` 계열이 삼각형마다 걸던 host rendezvous를, **순서 경계까지 모았다가 한
번에 넘기는** 경로로 바꿉니다. `REPIU_GLIDE_DRAW_BATCH` opt-in이며 **기본값은 꺼짐**
입니다.

**건드리지 않을 것:** 정점 디코드, 투영·깊이 매핑, 셰이더·combine, 생략 기구,
게이트 ABI(진입·검증·stdcall 정리·반환), 반환값이 있는 게이트.

## 2. 변경할 파일

| 파일 | 내용 |
|---|---|
| `include/repiu/platform/win32/glide_draw_batch.h` (신규) | 큐 상태·상한·flush 사유 열거, `ResolveGlideDrawBatchEnabled` |
| `src/platform/win32/telemetry/glide_draw_batch.cpp` (신규) | 큐 적재·flush 판정·통계 |
| `include/repiu/platform/win32/glide_opengl_backend.h` · `.cpp` | `DrawPrimitiveBatch(vertices, count, primitive)` — 한 쌍의 `glBegin`/`glEnd` |
| `src/platform/win32/boundary/linexe_glide_boundary.cpp` | draw 게이트는 큐에 적재, **draw가 아닌 게이트 진입 전 무조건 flush** |
| `src/host/win32/main.cpp` | 배치 통계 요약 로그(활성/배치 수/삼각형 수/평균 길이/최대/사유별) |
| `src/tools/aot_probe/glide_draw_batch_probe.{h,cpp}` (신규) · `main.cpp` · `CMakeLists.txt` | 분류·용량·primitive 전환·빈 flush 단정 |
| `docs/guides/glide-setter-elision-testing.md` · `README.md` | A/B 절차와 새 변수 |

## 3. 구현 규칙

* **flush 규칙은 하나입니다.** draw가 아닌 게이트를 처리하기 **전에** 비웁니다. 개별
  게이트를 열거하지 않습니다 — 빠뜨림이 곧 순서 오류이기 때문입니다.
* **큐는 값만 담습니다.** 게스트 포인터를 붙들지 않습니다.
* **primitive 종류가 바뀌면 비웁니다.** 삼각형·선·점·폴리곤을 한 배치에 섞지 않습니다.
* teardown과 창 닫기 경로에서 반드시 비웁니다.
* 진단(`REPIU_GLIDE_DRAW_DIAG`·tri census)은 **적재 시점**에 기록해 호출 순서 기준을
  유지합니다.
* 배치가 꺼져 있으면 지금과 **완전히 같은 경로**여야 합니다(삼각형당 rendezvous).

## 4. 검증

1. Win32 Debug 빌드 통과.
2. probe 통과 — draw 게이트만 큐잉, 그 밖 전부 flush, 용량 상한, primitive 전환,
   빈 큐 flush 무해.
3. 스모크 A/B(`=0`/`=1`) — 구현 공백 0 유지, **총 삼각형 수 일치**,
   `_GRDRAWTRIANGLE@12 count` 동일, 배치 통계의 삼각형 합 = draw 호출 수.
4. (사용자) A/B — `REPIU_GLIDE_SWAP_INTERVAL=0`,
   `REPIU_EXECUTION_TIME_PROFILE=1`, `REPIU_GLIDE_ORDINAL_TIME_PROFILE=1`을 **반드시**
   켜고 같은 구간 3회씩.

## 5. 완료 기준

1. `=0`이 지금과 동일하게 동작합니다.
2. `=1`에서 배치 평균 길이가 1보다 크고, 총 삼각형 수와 화면이 `=0`과 같습니다.
3. 요약 로그만으로 rendezvous 감소 배수를 읽을 수 있습니다.

---

# Task 438 Work Order — Glide draw batching, opt-in

## 1. Scope

Replace the per-triangle host rendezvous of the draw gates with a queue flushed **once per
ordering boundary**, behind `REPIU_GLIDE_DRAW_BATCH`, **off by default**. Not touched: vertex
decoding, projection and depth mapping, shaders and combine, the elision machinery, the gate ABI,
and any gate that returns a value.

## 2. Files

A new `glide_draw_batch` header and source hold the queue, its bound, the flush-reason
enumeration and the opt-in policy; the backend gains `DrawPrimitiveBatch` emitting N vertices
inside a single `glBegin`/`glEnd`; the boundary enqueues draw gates and **flushes before handling
any non-draw gate**; the loader prints batch statistics; a new probe plus its CMake and main.cpp
registration pins the rules; and the guide and README document the A/B and the variable.

## 3. Implementation rules

**One flush rule:** before handling any non-draw gate. No enumeration of individual gates, since
an omission is an ordering bug. The queue stores values only and never retains a guest pointer. A
change of primitive kind flushes, so triangles, lines, points and polygons never mix in one
batch. Teardown and window close must flush. Diagnostics record at enqueue time to preserve
call-order semantics. With the switch off the path must be exactly what it is today.

## 4. Verification

The Debug build passes; the probe pins the classification, the capacity bound, primitive
switching and the harmlessness of an empty flush; an `=0`/`=1` smoke keeps implementation gaps at
zero with **identical total triangle counts** and batch statistics summing to the draw call
count; and the user's A/B **must** enable `REPIU_GLIDE_SWAP_INTERVAL=0`,
`REPIU_EXECUTION_TIME_PROFILE=1` and `REPIU_GLIDE_ORDINAL_TIME_PROFILE=1`, three runs per
configuration on the same section.

## 5. Done when

`=0` behaves exactly as today, `=1` shows an average batch length above one with the same total
triangles and the same picture, and the summary log alone states the rendezvous reduction factor.
