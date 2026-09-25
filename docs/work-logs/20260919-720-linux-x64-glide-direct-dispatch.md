# Task 720 작업 로그 — Linux x64 Glide gate 직접 디스패치

설계: [20260919-720](../design/20260919-720-linux-x64-glide-direct-dispatch.md) ·
작업 지시: [20260919-720](../work-orders/20260919-720-linux-x64-glide-direct-dispatch.md) ·
frontier: [linux-port-frontier](../analysis/linux-port-frontier.md) ·
선행: [20260919-719](20260919-719-scene-timeline.md)

## 요약

**Linux x64가 Glide를 trap 없이 호출합니다.** 180초 실행에서 Glide 호출 348만 번이 모두
직접 디스패치로 성공했고, `ud2` boundary는 242만에서 0으로, 예외 처리 진입은 255만에서
12만으로 줄었습니다. 같은 180초에 그린 swap은 10,344에서 **15,832**로 늘었습니다.

그러나 **로딩 정지는 줄지 않았습니다**(합계 8.1초 → 8.2초). Task 719의 "정지는 Glide
trap 때문"이라는 추정은 **반증**됐습니다.

## 변경

* `RepiuLinuxX64GlideGateThunk`(`src/platform/linux/aot_dbt_glide_gate_thunk_x64.S`):
  gate의 `call`로 들어와 게스트 상태를 dispatch frame에 기록하고, `fxsave64` → `fninit`
  → 기본 MXCSR로 resolver를 부른 뒤 `fxrstor64`. 복귀 주소를 `R14D`에 넣어 기존 return
  thunk로 넘깁니다.
* `InstallLinuxX64GlideGateResolver`, `LinuxX64GlideGateThunkAddress`(platform)
* `ResolveLinuxX64GlideGateFrame`(engine): i386 resolver와 같은 검사로
  `HandleGlideGateBoundary`를 부르고 frame을 갱신
* x64에서 `GetGlideGateDirectDispatchThunkAddress()`가 thunk 주소를 돌려줌. resolver는
  게스트 진입 때 설치(`InstallGlideGateDirectDispatchResolver`)
* gate 패치(`E8 rel32; C2 n`)는 i386과 같은 것을 씀. x64에서는 thunk가 돌아오지 않으므로
  `C2 n`은 실행되지 않습니다.

## probe

`linux_x64_glide_gate_thunk`(실행형, `linux_x64_guest_register` 안): 배치된 페이지의 가짜
gate(`call thunk`)로 진입해 thunk → 가짜 Glide resolver → return thunk → 가짜 dispatch
resolver → 착지 `ret`까지 실제로 돕니다.

```text
linux_x64_glide_gate_thunk=true,seen_eip=0x20000000,seen_esp=0x20001100,fpu_tags=0x0,
mxcsr=0x1f80,dispatched=0x150000,eax=0x600d600d,esp=0x2000110c,x87_survived=true
```

thunk가 기록한 EIP(gate)·ESP, resolver 실행 중 빈 x87 스택과 기본 MXCSR, resolver의
EAX와 ESP 조정(+12)이 돌아옴, EBX 보존, 호출 쪽 x87 값 보존을 확인합니다.

## 측정 (Task 719와 같은 조건, 180초, 500ms 장면 표본)

| | Win32 | Linux 719 | Linux 720 |
|---|---:|---:|---:|
| 폴트 | 0 | 0 | 0 |
| Glide 직접 디스패치 성공 | 2,522,747 | 0 | **3,481,652** |
| `ud2` boundary | — | 2,425,507 | **0** |
| AOT boundary 합계 | 123,341 | 2,510,694 | 88,117 |
| 예외 처리 진입 | 522,651 | 2,548,911 | **123,063** |
| 180초 swap | 9,293 | 10,344 | **15,832** |
| swap 정지 합계 | 1.4초 | 8.1초 | 8.2초 |

swap 정지(표본 간격 700ms 초과)는 Linux 720에서 7.0초(1.6), 26.5초(2.8), 81.0초(0.9),
89.0초(1.8), 108.0초(3.3), 163.5초(0.8)로, 719와 같은 자리에 같은 길이로 남았습니다.
장면은 조금 일찍 옵니다(2주기 주황 인트로 86.5초 → 85.0초). 주기 길이는 Linux 82.5초,
Win32 76초입니다.

## 정지는 무엇인가 (미해결)

108초 정지 동안 `last_eip`는 여전히 `sti` 도우미(cache `0x20127C38`)이고, 예외 처리 진입은
초당 약 700, AOT 재진입은 초당 약 2,600에 그칩니다. 비용이 큰 경로가 아니므로 게스트는
**무언가를 기다리는 것**으로 보입니다. 시간 기준 대기(CD 오디오 위치, 사운드 재생 끝,
타이머 틱 수)가 후보이며, 확인하지 않았습니다.

## Win32

* Win32 x86 core probe **28/28**, 전체 빌드 오류 0. 새 코드는 모두
  `!_WIN32 && __x86_64__` 안입니다.
* 30초 실행: 폴트 0, 틱 카운터 14초 1,895 / 29초 5,505(Task 717 1,890 / 5,535), Glide
  직접 디스패치 target-miss·terminal 0. 잡음 범위입니다.

Linux x64 core probe **30/30**.

## 다음

로딩 정지의 정체. 정지 구간에 게스트가 기다리는 것을 찾습니다. 게스트 EIP 표본을 정지 중에
뜨는 방법(Linux에는 스레드 샘플링이 없음)부터 필요합니다.

---

## English

Design: [20260919-720](../design/20260919-720-linux-x64-glide-direct-dispatch.md) ·
Work order: [20260919-720](../work-orders/20260919-720-linux-x64-glide-direct-dispatch.md) ·
Frontier: [linux-port-frontier](../analysis/linux-port-frontier.md) ·
Predecessor: [20260919-719](20260919-719-scene-timeline.md)

### Summary

**Linux x64 now calls Glide without a trap.** In a 180-second run all 3.48 million
Glide calls succeeded through direct dispatch; `ud2` boundaries fell from 2.42
million to 0 and exception dispatches from 2.55 million to 123,000. Swaps over the
same 180 seconds rose from 10,344 to **15,832**.

**The loading stalls did not shrink**, however (8.1 s → 8.2 s in total). Task 719's
inference that the stalls come from Glide traps is **refuted**.

### Change

* `RepiuLinuxX64GlideGateThunk` (`src/platform/linux/aot_dbt_glide_gate_thunk_x64.S`):
  entered by the gate's `call`, records guest state in the dispatch frame, calls the
  resolver after `fxsave64` → `fninit` → default MXCSR, then `fxrstor64`, and hands
  the return address to the existing return thunk in `R14D`.
* `InstallLinuxX64GlideGateResolver` and `LinuxX64GlideGateThunkAddress` (platform).
* `ResolveLinuxX64GlideGateFrame` (engine): the i386 resolver's checks around
  `HandleGlideGateBoundary`, updating the frame.
* On x64 `GetGlideGateDirectDispatchThunkAddress()` returns the thunk; the resolver
  is installed at guest entry (`InstallGlideGateDirectDispatchResolver`).
* The gate patch (`E8 rel32; C2 n`) is the i386 one. The thunk does not return on
  x64, so the `C2 n` never runs.

### Probe

`linux_x64_glide_gate_thunk` (executing, within `linux_x64_guest_register`) enters a
fake gate (`call thunk`) on a placed page and really runs thunk → fake Glide
resolver → return thunk → fake dispatch resolver → landing `ret`. It checks the EIP
(the gate) and ESP the thunk recorded, an empty x87 stack and default MXCSR during
the resolver, the resolver's EAX and ESP adjustment (+12) coming back, EBX preserved,
and the caller's x87 value surviving:
`linux_x64_glide_gate_thunk=true,seen_eip=0x20000000,seen_esp=0x20001100,fpu_tags=0x0,mxcsr=0x1f80,dispatched=0x150000,eax=0x600d600d,esp=0x2000110c,x87_survived=true`.

### Measurement (Task 719's conditions, 180 s, 500 ms scene samples)

Win32 / Linux 719 / Linux 720: faults 0 / 0 / 0; direct Glide dispatch successes
2,522,747 / 0 / **3,481,652**; `ud2` boundaries — / 2,425,507 / **0**; AOT boundaries
123,341 / 2,510,694 / 88,117; exception dispatches 522,651 / 2,548,911 / **123,063**;
swaps in 180 s 9,293 / 10,344 / **15,832**; total swap stall 1.4 s / 8.1 s / 8.2 s.

The Linux 720 stalls (sample gap over 700 ms) are at 7.0 s (1.6), 26.5 s (2.8),
81.0 s (0.9), 89.0 s (1.8), 108.0 s (3.3) and 163.5 s (0.8) — the same places and
lengths as in 719. Scenes arrive slightly earlier (second-cycle orange intro 86.5 s →
85.0 s). The cycle is 82.5 s on Linux and 76 s on Win32.

### What the stalls are (open)

During the 108-second stall `last_eip` is still the `sti` helper (cache
`0x20127C38`), with about 700 exception dispatches and 2,600 AOT re-entries per
second — not an expensive path, so the guest appears to be **waiting for something**.
Candidates are time-based waits (CD audio position, end of a sound, a tick count);
none was checked.

### Win32

Win32 x86 core probe **28 of 28**, full build with no errors; all new code is inside
`!_WIN32 && __x86_64__`. A 30-second run: no faults, tick counter 1,895 at 14 s and
5,505 at 29 s (1,890 / 5,535 in Task 717), direct Glide dispatch with no target
misses or terminal failures — within noise. Linux x64 core probe **30 of 30**.

### Next

What the loading stalls are: find what the guest waits for during them. That first
needs a way to sample the guest EIP during a stall (Linux has no thread sampling).
