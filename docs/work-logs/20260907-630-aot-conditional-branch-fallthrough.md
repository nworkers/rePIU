# Task 630 작업 로그: AOT 조건 분기 fallthrough edge

설계: [20260907-630](../design/20260907-630-aot-conditional-branch-fallthrough.md) ·
작업 지시: [20260907-630](../work-orders/20260907-630-aot-conditional-branch-fallthrough.md) ·
분석: [linux-port-frontier 3.67](../analysis/linux-port-frontier.md)

## 한국어

### 작업 결과

`BuildAotCodeCacheImage`에서 조건 분기로 끝나는 블록의 not-taken edge를
필요할 때 emit하도록 고쳤습니다. 분기는 pending fallthrough를 남기고, 실제로
바이트를 만드는 다음 명령이 그 target이면 아무것도 emit하지 않으며, 다르거나
뒤에 아무것도 없으면 `E9`와 `kBlockFallthrough` fixup을 emit합니다. 조건이
없어 fail-closed `INT3`로 emit된 분기는 제외했습니다. 기존 `kCopy` fallthrough
규칙, fixup 해소 규칙, timer safe point 규칙은 그대로입니다.

`long_mode_emission` 코어 프로브에 회귀 항목 두 개를 추가했습니다.
`ARCHITECTURE.md`의 AOT emission 절에 새 규칙을 반영했습니다.

### 검증

빌드와 코어 프로브:

```text
cmake --build build/linux_x64 --target repiu_core_probe repiu -j 4
long_mode_emission_conditional_fallthrough_adjacent=true,reordered=true
long_mode_emission_all=true
core_probe_total=24
core_probe_failures=0
core_probe_all=true
```

edge 확인:

```text
[repiu-aot-map-fixup] source=0x0102A06C kind=conditional-branch target=0x01029F54 patch=0x0005C499 resolved=1
[repiu-aot-map-fixup] source=0x0102A06C kind=block-fallthrough target=0x0102A072 patch=0x0005C49E resolved=1
```

guest 실행:

```text
[repiu-fault] unhandled signal=0xb rip=0x200008cb eip=0x200008cb access=0x2a
  bytes=67 66 89 43 06 cc ... eax=0x5 ebx=0x24 edx=0x158cc40 esp=0x158cbf0
```

`0x011A6440` fault는 사라졌고, 다섯 번 연속 실행이 모두 같은 새 지점에서
멈췄습니다.

### 확인한 사실

* `0x0102A06C`가 taken edge와 block-fallthrough edge를 모두 갖게 되었습니다.
* 같은 실행에서 호출 루프가 정상 동작했습니다. `0x0102A082` return이
  `edx=0x0111128B`, `eax=1`, `status=0x010F1B20`으로 관측되었습니다. 이전에는
  `edx`가 `0`이고 비교가 항상 같음이었습니다.
* 새 frontier는 guest `0x010F6062`의 `MOV [EBX+6],AX`입니다. 원본 object 2에서
  이 인코딩은 그 한 곳뿐입니다. `EBX`가 far pointer 대상이 아니라 `0x24`이며,
  이는 segment HLE가 `PUSH ES`로 기록하는 ES selector 값입니다.
* 인접 fallthrough 항목이 통과하므로, fallthrough가 물리적으로 다음인
  image의 바이트는 바뀌지 않습니다. i386 배치도 같습니다.

### 판단

Task 628과 629가 확정한 결함이 수정되었고, 실행이 더 진행되어 다른 지점에서
멈춥니다. 새 지점은 이번 수정이 만든 것이 아니라 이전에는 도달하지 못했던
곳입니다. 남은 stack 정렬 문제는 해결하지 않았습니다.

### 다음 작업

`0x010F6062` 앞의 세 `POP EBX`가 소비하는 stack 내용을 추적해야 합니다.
`PUSH ES`/`POP ES` segment HLE의 stack 폭과 순서, 그리고 이 routine을 호출한
경로의 frame 구성이 확인 대상입니다.

## English

### Result

`BuildAotCodeCacheImage` now emits the not-taken edge of a block ending in a
conditional branch when it is needed. The branch leaves a pending fallthrough;
if the next instruction that actually emits bytes is its target, nothing is
emitted, and otherwise — or when nothing follows — an `E9` with a
`kBlockFallthrough` fixup is written. A branch emitted as a fail-closed `INT3`
for an unsupported condition is excluded. The existing `kCopy` fallthrough rule,
fixup resolution, and timer safe-point rule are unchanged.

Two regression items were added to the `long_mode_emission` core probe, and the
AOT emission section of `ARCHITECTURE.md` records the new rule.

### Verification

Build and core probe:

```text
cmake --build build/linux_x64 --target repiu_core_probe repiu -j 4
long_mode_emission_conditional_fallthrough_adjacent=true,reordered=true
long_mode_emission_all=true
core_probe_total=24
core_probe_failures=0
core_probe_all=true
```

The edge:

```text
[repiu-aot-map-fixup] source=0x0102A06C kind=conditional-branch target=0x01029F54 patch=0x0005C499 resolved=1
[repiu-aot-map-fixup] source=0x0102A06C kind=block-fallthrough target=0x0102A072 patch=0x0005C49E resolved=1
```

Guest execution:

```text
[repiu-fault] unhandled signal=0xb rip=0x200008cb eip=0x200008cb access=0x2a
  bytes=67 66 89 43 06 cc ... eax=0x5 ebx=0x24 edx=0x158cc40 esp=0x158cbf0
```

The `0x011A6440` fault is gone, and five consecutive runs all stopped at the
same new point.

### Facts established

* `0x0102A06C` now carries both its taken edge and a block-fallthrough edge.
* The calling loop behaves correctly in the same run: the return to
  `0x0102A082` was observed with `edx=0x0111128B`, `eax=1`, and
  `status=0x010F1B20`. Previously `edx` was `0` and every comparison reported
  equal.
* The new frontier is guest `0x010F6062`, `MOV [EBX+6],AX` — the only place that
  encoding appears in the original object 2. `EBX` is `0x24` rather than the far
  pointer it should be, and `0x24` is the ES selector value the segment HLE
  writes for `PUSH ES`.
* The adjacent-fallthrough item passes, so images whose fallthrough is
  physically next keep their bytes. That includes the i386 layout.

### Assessment

The defect Tasks 628 and 629 established is fixed, and execution now advances
and stops elsewhere. The new point is not created by this change; it is
somewhere the run could not previously reach. The remaining stack-alignment
problem is not solved.

### Next task

Trace the stack the three `POP EBX` instructions before `0x010F6062` consume.
The items to inspect are the width and ordering of the `PUSH ES` / `POP ES`
segment HLE and the frame built by the path that called this routine.
