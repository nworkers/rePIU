# 작업 기록 20260901-568 — x64 segment override를 실제 patcher에 연결

## 없던 것은 patcher가 아니었다

Task 567은 "patch하는 것이 없다"고 적었다. 틀렸다. patcher는 있었고
(`ReResolveWin32AotSegmentOverrides`), x64 slot에 맞지 않았을 뿐이다. 그 안에
이렇게 들어 있었다.

```cpp
bytes[site.cache_offset]      = 0x9CU;
bytes[site.cache_offset + 1U] = 0x66U;
bytes[site.cache_offset + 2U] = 0x81U;
bytes[site.cache_offset + 3U] = 0x3DU;
```

i386 emitter가 slot 앞에 쓴 바이트를 patcher가 **복제해 들고 있었다.** x64
slot은 lowered pushfd로 열리므로 저것을 적용하면 `41 5E 45`가 덮여 망가진다.

## 그 test가 실제로 잡는지 확인했다

말로 끝내지 않고 돌려봤다. patcher를 옛 상수로 되돌리자 probe는 **출력 한 줄도
못 내고 죽었다.** 되돌리니 다시 통과. test가 그 버그를 잡는다는 것이 확인됐다.

## 고친 방식 — 복제를 지운다

patcher가 상수를 갖는 대신 site가 `guard_prologue`를 들고 다닌다. 그리고 그
값은 각 emitter가 자기 바이트를 **다시 나열해서** 채우는 게 아니라 이미지에서
떠 온다. 두 번째 나열도 복제이기 때문이다.

## 이음매를 하나 찾았다

x64 probe가 진짜 patcher를 부르게 하려다 link가 깨졌다. `repiu_core_probe`는
플랫폼 계층 없이 모든 host에서 빌드되도록 만든 타깃인데, engine을 링크하면
OpenGL까지 딸려온다. Linux에는 engine을 링크하는 probe 타깃이 아예 없다
(`repiu_aot_probe`는 `if(WIN32)` 안이다).

그래서 이음매가 드러났다. **어떤 바이트를 쓸지는 emitter 옆에 있어야 할 runtime
지식이고, 어느 페이지를 열지는 engine의 일이다.** 둘이 한 함수에 있어서 앞의
것을 검증하려면 뒤의 것까지 통째로 링크해야 했다. 바이트 루프를
`src/runtime/aot_segment_patch.cpp`로 옮기고 engine에는 보호/flush만 남겼다.

## 검증

```
segment_patcher_sites    1
segment_patcher_native   1
segment_access_value     0xfeedface   (patcher가 base+disp를 직접 접었다)
segment_guard_boundary   1
segment_guard_no_access  0
segment_hle_routed       1
segment_hle_trapped      0xcc
segment_restore_native   1
segment_restored_value   0xfeedface
```

567은 접힌 주소를 내가 넣어줬지만 이번엔 patcher가 base로부터 직접 접는다.
그리고 `segment_hle_*` 세 줄이 이 단위의 핵심이다. 그 앞의 검사들은 slot 앞
바이트를 건드리지 않으므로 복원이 같은 값을 다시 쓰는 no-op이다. HLE로 한 번
보내야 `0xCC`가 찍히고, 되돌리는 일이 진짜 일이 된다.

## 내가 틀린 것 — 567의 예측 9

567은 census에 가정을 넣어 "patch되면 8 → 9"라고 적었다. 실제로 켜 보니
**11**이다.

가정이 틀린 게 아니라 **가정을 잘못 쟀다.** census는 "발행됨"을 두 곳에서
판단하는데(도달 walk의 술어, 그리고 tally 루프), 567의 가정은 그중 한 곳에만
적용됐다. tally는 여전히 segment override를 불완전으로 세어 `complete_blocks`를
깎았고, walk는 그 깎인 집합 위를 걸었다.

즉 이 단위가 engine에서 고친 것과 **똑같은 모양의 복제 버그가 내 측정 도구
안에도 있었다.** 그것도 하필 그 버그를 고치는 단위에서 만든 측정에.

## census를 규칙 공유로 바꿨다

`agrees=false`가 두 번 더 났다. 처음엔 tally가 segment override를 세지 않아서,
다음엔 census가 "segment override는 다 발행된다"고 가정해서 — 실제로는 65개 중
4개만 발행된다. emitter가 절대 disp32 한 가지 모양만 받고 FS/GS를 거부하기
때문이다.

그래서 `LongModeSegmentOverrideEmittable`을 내보내 **emitter와 census가 같은
함수를 부르게** 했다. 이 세션에서 세 번째로 같은 처방이다.

## 측정

```
              이전    이후
도달 가능      8      11
serviced      12      13
첫 정지    26 8b 1d   8e c0   (MOV ES, AX)
agrees      false    true
```

기본값은 켰다. 567이 끈 이유(patch하는 것이 없다)가 사라졌기 때문이다. slot은
여전히 실행 전에 patcher가 돌기를 요구하지만, 그것은 i386 slot이 늘 지고 있던
것과 같은 계약이다.

## 남은 것

- 다음 벽은 `8e c0`, guarded segment load다.
- `ReResolveWin32AotSegmentOverrides`의 이름에 `Win32`가 남아 있다. 호출부가
  많아 이 단위에 섞지 않았다.

---

# Work log 20260901-568 — connecting the x64 segment override to the real patcher

## What was missing was not a patcher

Task 567 recorded that "nothing patches them". That was wrong. The patcher
existed -- `ReResolveWin32AotSegmentOverrides` -- it just did not fit the x64
slot. Inside it:

```cpp
bytes[site.cache_offset]      = 0x9CU;
bytes[site.cache_offset + 1U] = 0x66U;
bytes[site.cache_offset + 2U] = 0x81U;
bytes[site.cache_offset + 3U] = 0x3DU;
```

The patcher held a **copy** of what the i386 emitter wrote at the head of the
slot. The x64 slot opens with a lowered `pushfd`, so applying that overwrites
`41 5E 45` and breaks it.

## Checked that the test actually catches it

Rather than asserting it, I ran it. With the patcher put back to the old
constant the probe **died without printing a single line**. Restored, it passes
again. The test catches the bug it was written for.

## The fix removes the copy

Instead of a constant in the patcher, the site carries `guard_prologue` -- and
that value is taken from the image rather than each emitter listing its own head
a second time, because a second listing is another copy.

## A seam worth finding

Wiring the x64 probe to the real patcher broke the link. `repiu_core_probe` is
built to stand up on every host with no platform layer, and linking the engine
drags OpenGL in. Linux has no probe target that links the engine at all
(`repiu_aot_probe` lives inside `if(WIN32)`).

That exposed the seam. **What bytes to write is runtime knowledge that belongs
beside the emitter; which page to open is the engine's business.** Having them
in one function meant verifying the first required linking all of the second.
The byte loop moved to `src/runtime/aot_segment_patch.cpp`; protection and
flushing stayed in the engine.

## Verification

```
segment_patcher_sites    1
segment_patcher_native   1
segment_access_value     0xfeedface   (the patcher folded base+disp itself)
segment_guard_boundary   1
segment_guard_no_access  0
segment_hle_routed       1
segment_hle_trapped      0xcc
segment_restore_native   1
segment_restored_value   0xfeedface
```

In 567 I supplied the folded address; here the patcher folds it from a base.
And the three `segment_hle_*` lines are the point of this unit: every check
before them leaves the slot's head untouched, so the restore rewrites the same
bytes and proves nothing. Routing to HLE first stamps `0xCC`, and only then does
going back do real work.

## What I got wrong -- 567's prediction of 9

Task 567 added a hypothesis to the census and recorded "8 -> 9 once patched".
Turned on for real, the answer is **11**.

The hypothesis was not wrong; **it was measured wrongly.** The census decides
"emitted" in two places -- the reachability walk's predicate and the tally loop
-- and 567's hypothesis was applied to only one of them. The tally still counted
segment overrides as incomplete, shrinking `complete_blocks`, and the walk then
walked the shrunken set.

So the same shape of duplication bug this unit fixed in the engine **was also
sitting in my own measuring tool** -- introduced, of all places, by the unit
that went on to fix it.

## The census now shares the rule

`agrees=false` appeared twice more: first because the tally did not count
segment overrides at all, then because the census assumed every segment override
is emitted. Only 4 of 65 are: the emitter admits one absolute `disp32` shape and
refuses FS and GS.

So `LongModeSegmentOverrideEmittable` is exported and **the emitter and the
census call the same function**. That is the third time this session the same
prescription has applied.

## Measurements

```
                 before   after
reachable          8       11
serviced          12       13
first stop     26 8b 1d   8e c0   (MOV ES, AX)
agrees          false     true
```

The default is on. The reason 567 kept it off -- nothing patched it -- is gone.
The slot still requires the patcher to run before the cache executes, but that
is the contract the i386 slot has always carried.

## What is left

- The next wall is `8e c0`, a guarded segment load.
- `ReResolveWin32AotSegmentOverrides` still has `Win32` in its name. It has many
  call sites and folding a rename into this unit would bury the real change.
