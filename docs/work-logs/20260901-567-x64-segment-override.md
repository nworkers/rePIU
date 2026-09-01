# 작업 기록 20260901-567 — x64 segment override slot

## 한 일

`EmitLongModeSegmentOverride`를 추가했다. slot은 세 개의 이미 만들어 둔
재작성을 조립한 것이다 — Task 559의 flags 시퀀스, Task 552의 SIB 절대주소
두 번. 새로 만든 것은 그것들을 잇는 guard뿐이다.

`ProbeSegmentOverride`는 selector 일치와 불일치를 **둘 다** 실행한다.

```
segment_access_value     0xfeedface   (일치: 접힌 base+disp가 옳은 곳에 닿음)
segment_esp_balanced     0x20001800
segment_guard_boundary   1            (불일치: boundary로 감)
segment_guard_no_access  0            (불일치: 접근이 실행되지 않음)
segment_guard_esp        0x20001800
```

`segment_guard_no_access`가 이 단위에서 가장 값이 큰 검사다. 일치하는 쪽만
봤다면 **guard가 아예 없는 slot과 구별되지 않는다**. 0이라는 것은 접근이
실행되지 않았다는 뜻이고, 그때야 guard가 지킨다고 말할 수 있다.

Linux x64 Release `core_probe_total=20 core_probe_failures=0`.

## 기본값을 false로 둔 이유

slot은 folded displacement와 guard 피연산자가 patch된 뒤에만 옳다. x64에는
아직 patch하는 것이 없다. 켜 두면 census의 도달 가능 block은 **늘어나면서
실행은 틀린다** — 지표만 좋아지고 물건은 나빠지는 정확한 형태다. 그래서
`enable_long_mode_segment_override = false`이고, probe가 스스로 patch한다.

## 틀린 것 하나

첫 실행이 죽었다. `PlaceImage`가 code page를 실행 전용으로 잠근 뒤에 patch를
쓰고 있었다. patch는 code에 대한 쓰기이므로 `kExecuteReadWrite` 창을 열고
쓴 뒤 instruction cache를 flush해야 한다. 엔진의 patcher도 같은 창이 필요하다
— 이 단위에서 미리 알게 된 것이 이득이다.

## 얼마나 열리는가

census에 별도 줄로 물었다.

```
reachable blocks     8
reachable if seg     9   (가정: segment override가 patch됨)
then stops at        0x10f4ca2   8e c0
```

**+1**이다. 그리고 다음에 막는 것은 `8e c0` = `MOV ES, AX`, 즉 segment
**load**다.

이 수치는 "segment override는 별로 안 중요하다"로 읽으면 안 된다. guest의
진입부는 DOS/4GW startup의 segment 설정 구간이고, override 바로 다음이 load
다. 하나만 만들면 한 칸이고, segment slot들을 묶어서 만들면 한참 나간다.
다음 단위를 고르는 근거가 이 줄에 있다.

## 남은 것

- 런타임 patcher가 없다. 그것이 생기기 전까지 headline 도달 가능 block은
  움직이지 않으며, 움직였다고 보고해서는 안 된다.
- FS/GS는 거부한 채로 둔다.

---

# Work log 20260901-567 — x64 segment override slot

## What was done

Added `EmitLongModeSegmentOverride`. The slot assembles three rewrites that
already existed -- Task 559's flags sequence and Task 552's SIB-absolute form,
twice. The only new thing is the guard that joins them.

`ProbeSegmentOverride` runs **both** the matching and the mismatching selector.

```
segment_access_value     0xfeedface   (match: folded base+disp reached the right data)
segment_esp_balanced     0x20001800
segment_guard_boundary   1            (mismatch: took the boundary)
segment_guard_no_access  0            (mismatch: the access did not run)
segment_guard_esp        0x20001800
```

`segment_guard_no_access` is the check worth the most here. Had I looked only
at the matching direction, the result would have been **indistinguishable from
a slot with no guard at all**. Zero means the access did not execute, and only
then can the guard be said to guard.

Linux x64 Release: `core_probe_total=20 core_probe_failures=0`.

## Why the default is false

The slot is correct only once its folded displacement and guard operands are
patched, and nothing on x64 patches them yet. Left on, it would **raise the
census's reachable-block count while making execution wrong** -- precisely the
shape where the metric improves and the artifact gets worse. Hence
`enable_long_mode_segment_override = false`, with the probe patching its own
image.

## One thing I got wrong

The first run crashed. I was writing the patches after `PlaceImage` had locked
the code page execute-only. A patch is a write to code, so it needs a
`kExecuteReadWrite` window and an instruction-cache flush afterwards. The
engine's patcher will need the same window -- learning it here was cheap.

## How much it unlocks

Asked as its own census line.

```
reachable blocks     8
reachable if seg     9   (hypothetical: segment overrides patched)
then stops at        0x10f4ca2   8e c0
```

**+1**, and the next thing to block is `8e c0` = `MOV ES, AX`, a segment
**load**.

This should not be read as "the segment override barely matters". The guest's
entry is the DOS/4GW startup's segment-setup run, so a load follows the
override immediately. Built alone it is worth one block; built together with
the other segment slots it goes much further. The line is the evidence for
choosing the next unit.

## What is left

- There is no runtime patcher. Until there is, the headline reachable-block
  count does not move, and must not be reported as though it had.
- FS/GS stay refused.
