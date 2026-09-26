# Task 743: Linux x64에서 번역되지 않은 return 대상 — 막다른 INT3를 fallback과 이름 있는 실패로

## 한국어

### 배경

Task 742의 검증기 오프셋 실수는 read 슬롯이 든 동적 이미지를 모두 거절했고, 그 빌드는 시작 직후
`[repiu-fault] unhandled signal=0x5`로 죽었다. 처음엔 arena 주소 `0x01104F87`에서, 재현 실행에서는
host 주소 `0x401F57FA`에서였다. 두 번째 주소는 `RepiuLinuxX64ReturnThunk`의
`.Lreturn_thunk_unresolved: int3`이다. 흐름은 이렇다.

1. 게임의 `call eax`(`0x01101E57`)가 돌아온 뒤 continuation `0x01101E59`(`pop edx …`)의 블록이
   동적 번역에서 거절된다.
2. callee의 `ret`이 return thunk를 거쳐 `LinuxX64EngineResolver`를 부른다. cache에 없고, 첫 명령
   `pop edx`(`5A`)는 long mode에서 8바이트 pop이라 "byte-identical"이 아니므로 resolver는 legacy
   resume도 거부하고 0을 돌려준다.
3. thunk는 `int3`로 끝나고, VEH는 host 주소의 breakpoint를 `kNoHostFrameToUnwind`로 거절한다.
   보고에는 이유가 없다.

간접 call의 같은 상황(`HandleAotReentry`의 `kAotReentryLegacyFallback`)은 다르다. 동일성 검사
없이 legacy fallback 플래그와 TF를 켜고 VEH의 단일 스텝 경로에 맡기며, 그 경로의 HLE·stack bridge가
push/pop을 에뮬레이트한다. `REPIU_AOT_DYNAMIC_REJECT=0x01104F87`로 `push ebx …` 블록을 거절해도
간접 call은 살아남는 이유다. return 경로만 첫 명령의 동일성을 요구했다.

### 설계

1. **재현 스위치.** `REPIU_AOT_DYNAMIC_REJECT=<guest 주소>`는 그 주소를 담은 동적 이미지를,
   `=read`는 guarded read site가 있는 이미지를 모두 worker가 거절하게 한다(`appended=0`, 메시지
   `rejected by REPIU_AOT_DYNAMIC_REJECT`). 번역 실패 시 엔진이 어떻게 하는지를 실패를 기다리지 않고
   본다.
2. **fault 보고에 마지막 VEH exit site.** 엔진이 `NoteVehExitSite`마다 platform 전역
   `repiu_last_veh_exit_site`/`_eip`에 쓰고, `[repiu-fault] unhandled …`가 `last_exit_site=`와
   `last_exit_eip=`를 찍는다.
3. **return resolver의 legacy fallback을 간접 call과 같은 조건으로.** 대상이 읽을 수 있는 guest 코드면
   `RepiuLinuxX64LegacyResumeThunk`로 보낸다. thunk는 TF를 켜고 `jmp`하므로 #DB가 대상에서 아무것도
   실행하기 전에 난다. 그 뒤는 VEH의 단일 스텝 경로가 간접 call과 같이 처리한다.
4. **남는 막다른 곳에 이름을.** 단일 스텝 경로가 HLE 뒤 continuation을 거절할 때와 마지막
   `kNoHostFrameToUnwind`에서 `[repiu-x64-untranslatable] stage= eip= bytes= …`를 찍는다(8회까지).
   Linux x64에는 인터프리터가 없어 "번역 없음 + 동일하지 않은 명령"은 여전히 실행할 수 없고, 그때는
   주소와 바이트를 남기고 죽는다.

### 검증 전략

`REJECT=read`로 죽는 지점이 return thunk에서 뒤로 물러나고 `[repiu-x64-untranslatable]`이 찍히는지,
스위치 없이 attract·플레이·pumpit2a가 그대로인지, core probe(Linux·Win32)가 통과하는지 본다.

## English

### Background

Task 742's validator offset mistake rejected every dynamic image holding a read slot, and that build
died right after start with `[repiu-fault] unhandled signal=0x5`: first at the arena address
`0x01104F87`, in the reproduction at the host address `0x401F57FA`, which is
`RepiuLinuxX64ReturnThunk`'s `.Lreturn_thunk_unresolved: int3`. The chain: the block at the
continuation `0x01101E59` (`pop edx …`) after the game's `call eax` (`0x01101E57`) is rejected by
dynamic translation; the callee's `ret` goes through the return thunk to `LinuxX64EngineResolver`,
which finds no cache entry and, because `pop edx` (`5A`) is an 8-byte pop in long mode and so not
"byte-identical", refuses the legacy resume too and returns 0; the thunk ends in `int3`, and the VEH
refuses a breakpoint at a host address as `kNoHostFrameToUnwind`, with no reason in the report.

The same situation on an indirect call (`HandleAotReentry`'s `kAotReentryLegacyFallback`) is
different: no identity check, the legacy-fallback flag and TF are set, and the VEH's single-step path
takes over, whose HLE and stack-bridge handlers emulate push/pop. That is why
`REPIU_AOT_DYNAMIC_REJECT=0x01104F87`, rejecting the `push ebx …` block, leaves an indirect call alive.
Only the return path demanded an identical first instruction.

### Design

1. **A reproduction switch.** `REPIU_AOT_DYNAMIC_REJECT=<guest address>` makes the worker reject any
   dynamic image holding that address, `=read` any image with a guarded read site (`appended=0`,
   message `rejected by REPIU_AOT_DYNAMIC_REJECT`), so what the engine does on a translation failure
   can be seen without waiting for one.
2. **The last VEH exit site in the fault report.** The engine writes the platform globals
   `repiu_last_veh_exit_site`/`_eip` at every `NoteVehExitSite`; `[repiu-fault] unhandled …` prints
   `last_exit_site=` and `last_exit_eip=`.
3. **The return resolver's legacy fallback under the indirect call's condition.** A readable guest
   target goes to `RepiuLinuxX64LegacyResumeThunk`, which sets TF and jumps, so the #DB lands at the
   target before anything there runs; from there the VEH's single-step path handles it as it does for
   an indirect call.
4. **Names for the dead ends that remain.** When the single-step path refuses a continuation after an
   HLE and at the final `kNoHostFrameToUnwind`, `[repiu-x64-untranslatable] stage= eip= bytes= …` is
   printed (up to eight times). Linux x64 has no interpreter, so "no translation and a non-identical
   instruction" still cannot run; it now dies naming the address and bytes.

### Verification strategy

With `REJECT=read` the death moves past the return thunk and prints `[repiu-x64-untranslatable]`;
without the switch, attract, play and pumpit2a are unchanged; the core probes (Linux and Win32) pass.

---

## 결과 (구현 후) / Result (after implementation)

### 한국어

설계 네 항목을 모두 넣었습니다. `REJECT=read` 재현에서 죽는 지점이 return thunk의 `int3`(host
주소, exit site `kNoHostFrameToUnwind`)에서 간접 call fallback 뒤의 `0x01104F8F`(`cmp dword [abs],0`,
비동일·번역 없음)로 물러났고, 거기서 `[repiu-x64-untranslatable]`이 주소와 바이트를 남깁니다.
스위치 없는 실행(attract·pumpit2a)과 core probe(Linux·Win32)는 그대로입니다. 설계와 달라진 점은
없습니다.

### English

All four items went in. In the `REJECT=read` reproduction the death moved from the return thunk's
`int3` (host address, exit site `kNoHostFrameToUnwind`) to `0x01104F8F` after the indirect-call
fallback (`cmp dword [abs],0`, non-identical, untranslated), where `[repiu-x64-untranslatable]` names
the address and bytes. Runs without the switch (attract, pumpit2a) and the core probes (Linux, Win32)
are unchanged. No departures from the design.
