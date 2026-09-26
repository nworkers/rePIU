# Task 743 작업 로그 — Linux x64에서 번역되지 않은 return 대상

설계: [20260926-743](../design/20260926-743-x64-untranslatable-return-target.md)
작업 지시: [20260926-743](../work-orders/20260926-743-x64-untranslatable-return-target.md)

## 요약

Task 742가 드러낸 잠복 결함을 잡았습니다. 동적 번역이 거절된 블록으로 `ret`이 돌아오면 return
thunk의 resolver가 legacy resume을 "첫 명령이 long mode에서 byte-identical일 때"만 허용해, 그 밖의
대상(`pop edx` 같은 스택 명령이 첫 명령인 흔한 경우)에서는 0을 돌려주고 thunk의 `int3`에서 이름 없는
`SIGTRAP`으로 죽었습니다. 간접 call의 같은 상황은 동일성 검사 없이 VEH의 단일 스텝 경로(HLE·stack
bridge)에 맡겨 살아남습니다. resolver를 그 조건에 맞췄고, 남는 막다른 곳(번역도 없고 동일하지도
않은 명령)은 `[repiu-x64-untranslatable] stage= eip= bytes= …`로 주소와 바이트를 남기고 죽게 했습니다.
재현은 새 스위치 `REPIU_AOT_DYNAMIC_REJECT`(주소 또는 `read`)로 결정적으로 합니다. fault 보고에
마지막 VEH exit site가 붙습니다.

## 과정

1. **결정적 재현.** `REPIU_AOT_DYNAMIC_REJECT=0x01104F87`(그 주소를 담은 이미지 거절)로는 죽지
   않았습니다: 간접 call `call eax → 0x01104F87`이 VEH의 `kAotReentryLegacyFallback`으로 `push ebx …`를
   HLE로 에뮬레이트하고 다음 블록(`0x01104F9C`)이 번역돼 이어졌습니다. `=read`(read site가 있는 이미지
   전부 거절, Task 742의 잘못된 검증기와 같은 상황)로는 재현됐습니다: `call eax`의 continuation
   `0x01101E59`(`5A C6 03 02 …`, `pop edx`) 블록이 거절되고, callee의 `ret`이 return thunk →
   `LinuxX64EngineResolver` → cache 없음, `pop edx`는 long mode에서 8바이트 pop이라 비동일 → 0 →
   `.Lreturn_thunk_unresolved: int3` → VEH가 host 주소 breakpoint를 `kNoHostFrameToUnwind`(0x2b)로
   거절 → `[repiu-fault] unhandled signal=0x5 rip=0x401F57FA`.
2. **fault 보고에 exit site.** `NoteVehExitSite`가 platform 전역 `repiu_last_veh_exit_site`/`_eip`에
   쓰고 `[repiu-fault] unhandled …`가 `last_exit_site=0x2b last_exit_eip=…`를 찍습니다. 이것으로 거절
   지점을 확정했습니다.
3. **resolver.** legacy resume 조건을 "byte-identical" 또는 "읽을 수 있는 guest 코드"로 넓혔습니다.
   `RepiuLinuxX64LegacyResumeThunk`는 TF를 켠 뒤 `jmp`하므로 #DB가 대상에서 아무것도 실행하기 전에
   나고, 그 뒤는 간접 call과 같은 VEH 경로입니다. core probe(`linux_x64_transfer_failure_provenance`
   포함) 통과.
4. **남는 막다른 곳.** `REJECT=read`로 다시 돌리면 return thunk에서는 죽지 않고 더 가서, 간접 call
   fallback이 `push ebx …`를 에뮬레이트한 뒤 `0x01104F8F`의 `83 3D 38 90 1B 01 00`(`cmp dword [abs],0`,
   절대 주소라 비동일)에서 번역 없이 멈춥니다. 두 거절 지점(`HandleSingleStepTrace`의 HLE 뒤 continuation
   거절, 마지막 `kNoHostFrameToUnwind`)에 `[repiu-x64-untranslatable]`을 넣어 주소·바이트·플래그·동적
   번역 시도 횟수를 남깁니다. Linux x64에는 인터프리터가 없으므로 이 경우는 여전히 실행할 수 없습니다.
   실제 실행에서는 동적 번역이 거절될 때만 생깁니다.

## 검증

| 검증 | 결과 |
|---|---|
| `REJECT=read` 재현 | 수정 전: return thunk `int3`(rip=0x401F57FA, exit site 0x2b)에서 사망. 수정 후: return 경로를 지나 `[repiu-x64-untranslatable] stage=single-step-trace eip=0x01104F8F bytes=83 3D 38 90 1B 01 00 0F`를 찍고 사망(설계대로) |
| `REJECT=0x01104F87` | 폴트 0(간접 call fallback이 처리) |
| pumpitea attract 30초(스위치 없음) | 폴트 0, 3,189 frame, breakpoint 819,608, VEH 7.15% |
| pumpit2a 25초 | 폴트 0, 3,641 frame |
| Linux x64 Release core probe | `core_probe_all=true` |
| Win32 x86 Debug 빌드 + core probe | 아래 English 절과 같음 |

로그: `build/task743-reject1..3.err.log`, `build/task743-bp1.err.log`, `build/task743-pumpit2a-reg.err.log`.

## 남은 것

* 번역 없고 비동일한 명령은 여전히 실행할 수 없습니다(이제 이름은 남습니다). 완전한 해결은 한 명령
  lowering 실행기나 인터프리터인데, 동적 번역이 정상이면 도달하지 않는 경로라 미룹니다.
* `unsafe_failure`의 `[repiu-aot-unsafe] line=N` 출력(Task 742)은 그대로 둡니다.

---

# English

# Task 743 work log — untranslated return targets on Linux x64

Design: [20260926-743](../design/20260926-743-x64-untranslatable-return-target.md)
Work order: [20260926-743](../work-orders/20260926-743-x64-untranslatable-return-target.md)

## Summary

The latent defect Task 742 exposed is fixed. When a `ret` came back to a block whose dynamic
translation had been rejected, the return thunk's resolver allowed the legacy resume only for a
byte-identical first instruction in long mode; any other target (commonly a stack instruction such as
`pop edx`) got 0, and the thunk's `int3` died as an anonymous `SIGTRAP`. The same situation on an
indirect call survives, because the VEH's single-step path (HLE and stack bridge) takes it with no
identity check. The resolver now uses that condition, and the dead ends that remain (no translation
and a non-identical instruction) die naming the address and bytes in
`[repiu-x64-untranslatable] stage= eip= bytes= …`. The new switch `REPIU_AOT_DYNAMIC_REJECT` (an
address or `read`) reproduces it deterministically, and the fault report carries the last VEH exit
site.

## Steps

1. **Deterministic reproduction.** `REPIU_AOT_DYNAMIC_REJECT=0x01104F87` (reject images holding that
   address) did not die: the indirect `call eax → 0x01104F87` went through the VEH's
   `kAotReentryLegacyFallback`, the HLE emulated `push ebx …`, and the next block (`0x01104F9C`)
   translated. `=read` (reject every image with a read site, the state of Task 742's wrong validator)
   reproduced it: the continuation `0x01101E59` (`5A C6 03 02 …`, `pop edx`) after `call eax` was
   rejected; the callee's `ret` went through the return thunk to `LinuxX64EngineResolver`: no cache
   entry, `pop edx` is an 8-byte pop in long mode and so not identical, 0 returned,
   `.Lreturn_thunk_unresolved: int3`, and the VEH refused a breakpoint at a host address as
   `kNoHostFrameToUnwind` (0x2b): `[repiu-fault] unhandled signal=0x5 rip=0x401F57FA`.
2. **Exit site in the fault report.** `NoteVehExitSite` writes the platform globals
   `repiu_last_veh_exit_site`/`_eip`, and `[repiu-fault] unhandled …` prints `last_exit_site=0x2b
   last_exit_eip=…`, which settled the refusal point.
3. **Resolver.** The legacy resume condition is now "byte-identical" or "readable guest code".
   `RepiuLinuxX64LegacyResumeThunk` sets TF and jumps, so the #DB lands at the target before anything
   there runs; from there it is the indirect call's VEH path. The core probe, including
   `linux_x64_transfer_failure_provenance`, passes.
4. **The dead ends left.** With `REJECT=read` again, the run passes the return thunk, the indirect-call
   fallback emulates `push ebx …`, and it stops at `0x01104F8F`'s `83 3D 38 90 1B 01 00` (`cmp dword
   [abs],0`, absolute addressing, not identical) with no translation. Both refusal points (the
   continuation refusal after an HLE in `HandleSingleStepTrace`, and the final `kNoHostFrameToUnwind`)
   print `[repiu-x64-untranslatable]` with the address, bytes, flags and dynamic-translation attempt
   count. Linux x64 has no interpreter, so this case still cannot run; in real runs it arises only
   when dynamic translation is rejected.

## Verification

| Check | Result |
|---|---|
| `REJECT=read` reproduction | before: death at the return thunk's `int3` (rip=0x401F57FA, exit site 0x2b); after: past the return path, `[repiu-x64-untranslatable] stage=single-step-trace eip=0x01104F8F bytes=83 3D 38 90 1B 01 00 0F` and death as designed |
| `REJECT=0x01104F87` | no fault (the indirect-call fallback handles it) |
| pumpitea 30 s attract, no switch | no faults, 3,189 frames, 819,608 breakpoints, VEH 7.15% |
| pumpit2a 25 s | no faults, 3,641 frames |
| Linux x64 Release core probe | `core_probe_all=true` |
| Win32 x86 Debug build + core probe | build succeeded, `core_probe_all=true` |

Logs: `build/task743-reject1..3.err.log`, `build/task743-bp1.err.log`,
`build/task743-pumpit2a-reg.err.log`.

## What remains

* An instruction with no translation and no long-mode identity still cannot run (it is now named). A
  full answer is a single-instruction lowering executor or an interpreter; the path is unreachable
  while dynamic translation works, so it is deferred.
* Task 742's `[repiu-aot-unsafe] line=N` prints at the `unsafe_failure` setters stay.
