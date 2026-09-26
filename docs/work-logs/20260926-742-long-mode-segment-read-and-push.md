# Task 742 작업 로그 — long mode에서 `mov r,sreg`와 `push sreg`를 trap 없이

설계: [20260926-742](../design/20260926-742-long-mode-segment-read-and-push.md)
작업 지시: [20260926-742](../work-orders/20260926-742-long-mode-segment-read-and-push.md)

## 요약

Task 741의 census에서 breakpoint의 10%를 내던 memcpy helper의 `push es`·`mov eax,ds`가 Linux x64
AOT cache에서 trap 없이 돕니다. 두 명령의 값은 guest 눈에 보이는 selector이고 엔진은 그것을
shadow(`shadow_selectors->selectors[seg]`)에 유지하므로, long-mode 슬롯은 guard 없이 shadow를 읽습니다.
`mov r16/r32, sreg`(`kGuardedSegmentRead`)는 15/16바이트 슬롯, `push sreg`는 새 kind
`kGuardedSegmentPush`의 23바이트 슬롯입니다. pumpitea attract 30초에서 두 지점이 census에서 사라졌고
(0회), breakpoint 878,285회(Task 741의 1.02M~1.22M), VEH 벽시계 비중 6.65%(9%대)입니다. pumpit2a는
breakpoint 149,911 → 107,756회입니다.

## 과정

1. **원인 확인.** `push es`(`06`)는 long mode에서 무효 opcode라 HLE boundary(INT3)였고, `mov eax,ds`는
   `kGuardedSegmentRead`로 분류되지만 long-mode 방출기가 없었습니다. 그런데 runtime 옵션
   `enable_guarded_segment_read`가 켜져 있어 generic switch가 **i386 read 슬롯을 long-mode 이미지에
   방출**하고 있었습니다: `9C`(pushfq, host 스택), `66 8C C0`(host DS=0), `66 3B 05 disp32`(long mode에선
   RIP 상대) → 항상 mismatch → fallback INT3. 매번 trap하되 host 스택은 복원되어 "동작"한 셈입니다.
2. **슬롯.** read: `67 0F B7 /r 25 disp32`(32비트, 상위 16비트 0) 또는 `67 66 8B /r 25 disp32 90`(16비트),
   disp32는 +5, `E9` fallthrough, `CC` fallback. push: `lea r15d,[r15-4]`, `movzx r14d, word [shadow]`,
   `mov [r15], r14d`, `E9`, `CC`. site는 `AotGuardedSegmentReadSite`를 재사용하고 두 주소 오프셋을 같은
   슬롯으로 두어 기존 patcher가 그대로 채웁니다. generic switch는 long mode에서 i386 read 슬롯을 더
   이상 방출하지 않고(거부 시 INT3), i386 이미지의 `push sreg`는 원본 복사입니다.
3. **planner.** `ReadGuardedSegmentPushRegister`(`06`/`1E`/`0F A0`/`0F A8`, operand 32비트).
   `66 06`은 push로 잡지 않습니다.
4. **검증기 오프셋 실수와 그 결과.** 첫 판의 `ValidateAotCodeCacheHleCoverage`가 read 슬롯의 disp32를
   +4로 기대했습니다(실제 +5: `67`+opcode 2+modrm+SIB). probe가 `bytes=false, coverage=false`로 잡았고,
   그 빌드로 게임을 돌리면 시작 직후 `SIGTRAP unhandled rip=0x01104F87`로 죽었습니다. 경위: read 슬롯이
   든 동적 이미지가 coverage에서 거절돼 그 블록이 번역되지 않고, `call eax`(0x01101E57)가 그 미번역
   대상으로 가면 legacy fallback이 TF를 켜고 arena로 가는데 그 첫 단일 스텝을 VEH가 거절합니다.
   오프셋을 고치자 4/4 실행 통과. **미번역 간접 call 대상에서 단일 스텝이 거절되는 것은 잠복 결함**으로
   남깁니다(이번 작업 범위 밖).
5. `unsafe_failure`를 켜는 7곳에 `[repiu-aot-unsafe] line=N` 출력을 남겼습니다(치명 경로에서만 찍힘).

## 검증

| 검증 | 결과 |
|---|---|
| Linux x64 Release core probe | `core_probe_all=true`. 새 case: `mov eax,ds`·`mov bx,ds`·`push es` 슬롯 바이트·site·coverage·손상 거부, `66 06` push 거부 |
| pumpitea attract 30초 | 폴트 0, `0x010FE375`/`0x010FE376` 0회, breakpoint 878,285(741: 1.02M~1.22M), VEH 6.65%(741 뒤 8.5~9.7%), 3,207 frame, read site 111개 |
| pumpitea 플레이 60초(합성 키) | 곡 `39.AUD` 도달, MP3 multi 0·pcm-empty 0, 폴트 0 |
| pumpit2a 25초 | 폴트 0, 3,677 frame, breakpoint 107,756(741 회귀 실행 149,911) |
| 재현성 | 오프셋 수정 빌드 4/4 실행 폴트 0 |
| Win32 x86 Debug 빌드 + core probe | 아래 English 절과 같음 |

## 남은 것

* 미번역 간접 call 대상으로의 legacy fallback에서 첫 단일 스텝이 `unhandled`로 죽는 잠복 결함
  (검증기 실수가 드러냄). 동적 이미지가 거절되는 다른 이유가 생기면 다시 나타납니다.
* `jmp cs:[table]`(0x010F659E, 초당 1,200회)은 여전히 boundary입니다.
* 남은 상위 지점은 모두 `delay()`·`lseek`·ISR의 INT 21h/privileged 명령입니다.

---

# English

# Task 742 work log — `mov r,sreg` and `push sreg` without a trap in long mode

Design: [20260926-742](../design/20260926-742-long-mode-segment-read-and-push.md)
Work order: [20260926-742](../work-orders/20260926-742-long-mode-segment-read-and-push.md)

## Summary

The memcpy helper's `push es` and `mov eax,ds`, 10% of the breakpoints in Task 741's census, now run
in the Linux x64 AOT cache without a trap. Both produce the guest-visible selector, which the engine
keeps in the shadow (`shadow_selectors->selectors[seg]`), so the long-mode slots read the shadow with no
guard: a 15/16-byte slot for `mov r16/r32, sreg` (`kGuardedSegmentRead`) and a 23-byte slot for
`push sreg` (new kind `kGuardedSegmentPush`). In pumpitea's 30 s attract the two sites are gone from
the census (zero), breakpoints are 878,285 (1.02–1.22 million in Task 741) and the VEH's share of wall
6.65% (about 9%). pumpit2a went from 149,911 to 107,756 breakpoints.

## Steps

1. **Cause.** `push es` (`06`) is an invalid opcode in long mode and stayed an HLE boundary (INT3);
   `mov eax,ds` was classified `kGuardedSegmentRead` but had no long-mode emitter. With the runtime
   option `enable_guarded_segment_read` on, the generic switch was **emitting the i386 read slot into
   long-mode images**: `9C` (pushfq on the host stack), `66 8C C0` (host DS = 0), `66 3B 05 disp32`
   (RIP-relative in long mode), always a mismatch, so the fallback INT3 every time; it "worked" because
   the host stack was restored.
2. **Slots.** Read: `67 0F B7 /r 25 disp32` (32-bit, upper half zeroed) or `67 66 8B /r 25 disp32 90`
   (16-bit), displacement at +5, an `E9` fallthrough and a `CC` fallback. Push: `lea r15d,[r15-4]`,
   `movzx r14d, word [shadow]`, `mov [r15], r14d`, `E9`, `CC`. The site reuses
   `AotGuardedSegmentReadSite` with both address offsets on the same slot, so the existing patcher
   fills it. The generic switch no longer emits the i386 read slot in long mode (a refusal closes with
   INT3), and i386 images copy `push sreg` verbatim.
3. **Planner.** `ReadGuardedSegmentPushRegister` (`06`/`1E`/`0F A0`/`0F A8`, 32-bit operand); `66 06`
   is not admitted.
4. **A validator offset mistake and what it exposed.** The first `ValidateAotCodeCacheHleCoverage`
   expected the read slot's displacement at +4 (it is +5: `67` plus a two-byte opcode, modrm and SIB).
   The probe caught it (`bytes=false, coverage=false`), and that build died right after start with
   `SIGTRAP unhandled rip=0x01104F87`: dynamic images holding a read slot failed coverage, the block
   stayed untranslated, and when `call eax` (0x01101E57) reached that target the legacy fallback armed
   TF into the arena and the VEH refused the first single step. With the offsets fixed, 4 of 4 runs
   pass. **The refused single step at an untranslated indirect-call target is a latent defect**, left
   outside this task.
5. The seven `unsafe_failure` setters now print `[repiu-aot-unsafe] line=N` (fatal path only).

## Verification

| Check | Result |
|---|---|
| Linux x64 Release core probe | `core_probe_all=true`; new cases: `mov eax,ds`, `mov bx,ds`, `push es` slot bytes, site, coverage and corruption rejection; `66 06` refused as a push |
| pumpitea 30 s attract | no faults, `0x010FE375`/`0x010FE376` zero, 878,285 breakpoints (741: 1.02–1.22 M), VEH 6.65% (8.5–9.7% after 741), 3,207 frames, 111 read sites |
| pumpitea 60 s play (synthetic keys) | reaches song `39.AUD`, MP3 multi 0 and pcm-empty 0, no faults |
| pumpit2a 25 s | no faults, 3,677 frames, 107,756 breakpoints (149,911 in the 741 regression run) |
| Reproducibility | 4 of 4 runs of the fixed build without a fault |
| Win32 x86 Debug build + core probe | build succeeded, `core_probe_all=true` |

## What remains

* The latent defect above: a legacy fallback to an untranslated indirect-call target dies on its first
  single step as `unhandled`. Any other reason for a dynamic image to be rejected would bring it back.
* `jmp cs:[table]` (0x010F659E, 1,200 a second) is still a boundary.
* The remaining top sites are all `delay()`, `lseek` and the ISR's INT 21h and privileged instructions.
