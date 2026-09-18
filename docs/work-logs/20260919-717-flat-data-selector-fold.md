# Task 717 작업 로그 — loader 데이터 선택자도 flat으로 fold

설계: [20260919-717](../design/20260919-717-flat-data-selector-fold.md) ·
작업 지시: [20260919-717](../work-orders/20260919-717-flat-data-selector-fold.md) ·
frontier: [linux-port-frontier](../analysis/linux-port-frontier.md) ·
선행: [20260918-716](20260918-716-long-mode-cs-data-boundary.md)

## 요약

**safe point 주입을 켠 Linux x64가 이제 폴트 없이 달립니다.** 30초, 40초, 90초 실행
모두 폴트가 없었고, 90초 동안 3,913프레임을 그리며 장면이 계속 바뀝니다. 힙을 덮던
것은 `sprintf("%d")`가 **ES에 실린 loader 데이터 선택자 `0x0024`의 base
`0x01010000`**을 더해 스택 대신 텍스처 픽셀을 읽은 결과였습니다. Task 712의 SS 규칙을
모든 segment 레지스터와 데이터 선택자로 넓혀 고쳤습니다. 주입은 아직 opt-in입니다.

## 추적

1. **어떤 구조체인가** — 폴트 스택의 저장된 `esi`가 GL context `0x01802BD0`,
   `ctx->Shared`는 `0x0158DAD8`. 매초 peek하니 28–29초 사이에 블록 전체가
   `0xF186F186`로 바뀝니다. Win32는 같은 구조체(8바이트 아래)가 40초 동안 그대로입니다.
2. **fill의 모양** — `REPIU_LIVE_GUEST_SCAN`: `0x0158CBF0`(스택 꼭대기 `0x0158CC90`
   바로 아래)부터 위로, 0x200 간격의 구간들로 번집니다. 크래시 때 스택
   덤프(`[repiu-fault-stack]`)에서 반환 주소 `0x0102D6F0` 위의 호출자 프레임이 전부
   `0xF186F186`입니다.
3. **누가 쓰는가** — `REPIU_LINUX_X64_DATA_WATCH=0x0158DAE0`(`Shared+8`). 초기화
   쓰기 뒤에 `mov [edx],bl`이 `86`, `F1`을 번갈아 한 바이트씩 씁니다. 게스트
   `0x010F0DF5`, Watcom `sprintf`의 출력 콜백이고, 스택의 남은 개수는
   `0x5353, 0x5352, …`로 줄어듭니다. 호출자는 `__prtf`의 숫자 복사 루프
   `0x010F35B3`(`mov dl, es:[ebx]`)입니다.
4. **두 host 비교** — `0x010F3536`에서 arrival probe:
   Win32 `ebx=0x0458CA90 edx=0x002B`, Linux `ebx=0x0158CA98 edx=0x0024`. 버퍼는 둘 다
   스택이고, ES로 갈 값만 다릅니다. Win32 게스트는 DS를 읽으면 host의 flat 선택자를,
   Linux 게스트는 loader 선택자 `0x0024`를 얻습니다.

## 수정

* `ApplyFlatSegmentFolds` — ES, SS, DS, FS, GS에서 초기 스택 선택자(`0x0034`)나 초기
  데이터 선택자(`0x0024`)를 가진 항목은 fold base 0. 다른 선택자는 descriptor base
  유지(Task 712와 같음)
* `ThreadContext::flat_data_selector` = 초기 `guest_ds`
* 진단 세 가지(설정하지 않으면 무동작): `REPIU_LIVE_GUEST_SCAN`,
  `[repiu-fault-stack]`, `REPIU_LINUX_X64_DATA_WATCH`

## 검증

* Linux x64 core probe **30/30**, Win32 x86 core probe **28/28**, Win32 전체 빌드
  오류 0. `flat_segment_fold_table=true`
* Linux 주입 on 30초: 폴트 0, 틱 카운터 29초 5,129, safe point 주입 3,116/3,229,
  전체 틱 5,364/5,487
* Linux 주입 on 40초: 폴트 0, fill 없음
* Linux 주입 on 90초: 폴트 0, 3,913프레임, safe point 주입 8,545/8,650, 장면이 계속
  바뀌고 전체 화면 장면(약 307,000픽셀)까지 진행
* Linux 주입 off 30초: 폴트 0, 시계가 17초부터 1,754에서 정지 — 이전과 같음
* Win32 30초: 폴트 0. 이전 실행 대비 틱 카운터 29초 5,535(이전 5,579), safe point
  5,703/5,753(이전 5,803/5,852), guarded segment-pop 24,613/3,270(이전
  24,493/3,274). 잡음 범위입니다.

### 화면 진행 (`REPIU_GLIDE_PIXEL_DIAG=1`, 주입 on)

주황 페이드가 Win32보다 **적은 swap 수**에 끝납니다(Linux swap 38 무렵 최대 밝기,
Win32 60 이후). Linux는 초당 swap이 적지만 게임 시계는 벽시계를 따르므로, swap
하나당 진행이 큽니다. 시간 기준 애니메이션이 제대로 돈다는 뜻입니다. 두 번째 장면
`(190,155,93)`은 Linux swap 400–800, 이후 장면들이 이어집니다.

## 스크립트 함정

`REPIU_LINUX_X64_SAFE_POINT_INJECTION`은 **값과 무관하게 존재만 확인**합니다. 처음
"off" 실행을 `=0`으로 돌려 사실상 on을 측정했고, 변수를 빼고 다시 돌렸습니다.

## 다음

1. **주입을 기본값으로 켜기.** 이번 수정으로 on이 30–90초 안정적이고, Win32는 원래
   이 경로로 틱을 넣습니다. off의 "렌더 단계 시계 정지"를 없애는 마지막 단계입니다.
2. 게스트가 세그먼트 레지스터를 읽을 때 두 host가 다른 값을 돌려주는 문제 자체는
   남아 있습니다(Win32 `0x2B`, Linux `0x0024`). 이번 수정은 그 값을 fold에서 같게
   취급할 뿐입니다.

---

## English

Design: [20260919-717](../design/20260919-717-flat-data-selector-fold.md) ·
Work order: [20260919-717](../work-orders/20260919-717-flat-data-selector-fold.md) ·
Frontier: [linux-port-frontier](../analysis/linux-port-frontier.md) ·
Predecessor: [20260918-716](20260918-716-long-mode-cs-data-boundary.md)

### Summary

**Linux x64 with safe-point injection on now runs without faults**: none in 30-,
40- and 90-second runs, and over 90 seconds it draws 3,913 frames with the scenes
changing throughout. The heap was being overwritten because `sprintf("%d")` added
**the base `0x01010000` of loader data selector `0x0024`, loaded into ES**, and read
texture pixels instead of the stack. Task 712's SS rule was widened to every segment
register and to the data selector. Injection is still opt-in.

### Trace

1. **Which structure**: the saved `esi` in the fault stack is the GL context
   `0x01802BD0`, whose `Shared` is `0x0158DAD8`. Peeked each second, the whole block
   turns into `0xF186F186` between 28 and 29 seconds; on Win32 the same structure
   (8 bytes lower) is untouched for 40 seconds.
2. **The fill's shape**: `REPIU_LIVE_GUEST_SCAN` shows it spreading up from
   `0x0158CBF0`, just under the stack top `0x0158CC90`, in runs 0x200 apart. At the
   crash the stack dump (`[repiu-fault-stack]`) shows every caller frame above the
   return address `0x0102D6F0` as `0xF186F186`.
3. **The writer**: `REPIU_LINUX_X64_DATA_WATCH=0x0158DAE0` (`Shared+8`). After the
   initializing write, `mov [edx],bl` writes `86` and `F1` alternately, a byte at a
   time: guest `0x010F0DF5`, Watcom `sprintf`'s output callback, with the remaining
   count on the stack falling `0x5353, 0x5352, …`. Its caller is `__prtf`'s number
   copy loop `0x010F35B3` (`mov dl, es:[ebx]`).
4. **Both hosts**: an arrival probe at `0x010F3536` gives Win32
   `ebx=0x0458CA90 edx=0x002B` and Linux `ebx=0x0158CA98 edx=0x0024`. Both buffers
   are on the stack; only the value bound for ES differs. Reading DS, the Win32 guest
   gets the host's flat selector and the Linux guest gets loader selector `0x0024`.

### Fix

* `ApplyFlatSegmentFolds`: in ES, SS, DS, FS and GS, an entry holding the initial
  stack selector (`0x0034`) or the initial data selector (`0x0024`) folds base 0;
  other selectors keep their descriptor base, as in Task 712.
* `ThreadContext::flat_data_selector` = the initial `guest_ds`.
* Three diagnostics, inert when unset: `REPIU_LIVE_GUEST_SCAN`,
  `[repiu-fault-stack]`, `REPIU_LINUX_X64_DATA_WATCH`.

### Verification

* Linux x64 core probe **30 of 30**, Win32 x86 **28 of 28**, full Win32 build with
  no errors; `flat_segment_fold_table=true`.
* Linux, injection on, 30 s: no faults; tick counter 5,129 at 29 s; safe-point
  injections 3,116 of 3,229; ticks 5,364 of 5,487.
* Linux, injection on, 40 s: no faults, no fill.
* Linux, injection on, 90 s: no faults, 3,913 frames, safe-point injections 8,545 of
  8,650, scenes changing throughout and reaching full-screen scenes (about 307,000
  pixels).
* Linux, injection off, 30 s: no faults; the clock stops at 1,754 from 17 s, as
  before.
* Win32, 30 s: no faults. Against the previous run: tick counter 5,535 at 29 s
  (5,579), safe points 5,703 of 5,753 (5,803 of 5,852), guarded segment-pop
  24,613/3,270 (24,493/3,274) — within noise.

#### Screen progression (`REPIU_GLIDE_PIXEL_DIAG=1`, injection on)

The orange fade finishes in **fewer swaps** than on Win32 (full brightness near swap
38 on Linux, after 60 on Win32). Linux swaps less often per second but its game
clock follows wall time, so each swap advances further — time-based animation is
working. The second scene `(190,155,93)` shows at Linux swaps 400–800, and later
scenes follow.

### Script pitfall

`REPIU_LINUX_X64_SAFE_POINT_INJECTION` is checked **for presence only, whatever its
value**. The first "off" run set it to `0` and so measured on; it was rerun with the
variable removed.

### Next

1. **Turn injection on by default.** With this fix it is stable for 30 to 90
   seconds, and it is how Win32 delivers ticks. It is the last step to removing the
   off-mode "clock stops in the render phase".
2. The two hosts still return different values when the guest reads a segment
   register (Win32 `0x2B`, Linux `0x0024`); this fix only makes the fold treat them
   alike.
