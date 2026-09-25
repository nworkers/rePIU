# Task 718 작업 로그 — Linux x64 safe point 틱 주입을 기본값으로

설계: [20260919-718](../design/20260919-718-default-safe-point-injection.md) ·
작업 지시: [20260919-718](../work-orders/20260919-718-default-safe-point-injection.md) ·
frontier: [linux-port-frontier](../analysis/linux-port-frontier.md) ·
선행: [20260919-717](20260919-717-flat-data-selector-fold.md)

## 요약

**Linux x64의 게임 시계가 이제 기본 설정으로 흐릅니다.** safe point 틱 주입을 기본값으로
켰고, `REPIU_LINUX_X64_SAFE_POINT_INJECTION=0`일 때만 끕니다. 180초 실행이 폴트 없이
9,994프레임을 그렸고, 게임의 attract 주기는 Win32와 거의 같은 속도로 돕니다.

## 변경

* `runtime::ResolveTimerSafePointInjection`(헤더 전용): 변수가 없거나 `0`이 아니면 on
* trampoline의 x64 주입 경로가 이 함수를 씀. 이전에는 존재만 검사해 `=0`도 on이었음
* probe `timer_safe_point_injection_setting`: 없음·`1`·빈 문자열 on, `0` off

## 검증

* Linux x64 core probe **30/30**, Win32 x86 core probe **28/28**, Win32 전체 빌드 오류 0
* 변경은 `#if defined(__x86_64__)` 안이라 Win32 x86 바이너리의 동작은 바뀌지 않습니다.

### Linux 30초

| | 기본값(변수 없음) | `=0` |
|---|---|---|
| 폴트 | 0 | 0 |
| 틱 카운터 10 / 17 / 20 / 29초 | 438 / 2,079 / 2,749 / 4,848 | 412 / 1,752 / 1,754 / 1,754 |
| safe point trap/주입/보류 | 3,073 / 2,952 / 121 | 3,923 / 0 / 3,923 |

`=0`은 Task 715와 같이 17초부터 시계가 멈춥니다.

### 180초, 두 host

| | Win32 | Linux 기본값 |
|---|---|---|
| 폴트 | 0 | 0 |
| 틱 카운터 초기화 | 33초(6,406→317), 110초(18,566→470) | 42초(8,064→387), 126초(19,821→333) |
| 초기화 간격 | 77초 | 84초 |

Linux: 9,994프레임, safe point 주입 15,660/15,779. 틱 카운터는 게임이 주기마다
되돌리는 값이라, 두 host 모두 같은 자리에서 초기화됩니다. Linux가 늦게 출발하는 것은
자산 디코드가 느리기 때문이고(Task 715), 주기 간격은 9% 차이입니다.

## 다음

* 주기 간격의 9% 차이와 첫 초기화 전 카운터가 더 큰 것(8,064 대 6,406)은 Linux가 같은
  장면에 더 오래 머문다는 뜻일 수 있습니다. 장면별 시각을 두 host에서 맞춰 보는 것이
  다음 비교입니다.
* 게스트가 세그먼트 레지스터를 읽을 때 두 host가 다른 값을 돌려주는 문제(Task 717)는
  남아 있습니다.

---

## English

Design: [20260919-718](../design/20260919-718-default-safe-point-injection.md) ·
Work order: [20260919-718](../work-orders/20260919-718-default-safe-point-injection.md) ·
Frontier: [linux-port-frontier](../analysis/linux-port-frontier.md) ·
Predecessor: [20260919-717](20260919-717-flat-data-selector-fold.md)

### Summary

**The Linux x64 game clock now runs with default settings.** Safe-point tick
injection is on by default and off only with
`REPIU_LINUX_X64_SAFE_POINT_INJECTION=0`. A 180-second run drew 9,994 frames with
no faults, and the game's attract cycle turns at nearly Win32's pace.

### Change

* `runtime::ResolveTimerSafePointInjection` (header-only): on when the variable is
  unset or anything but `0`.
* The trampoline's x64 injection path uses it; before, the variable was tested for
  presence and `=0` meant on.
* Probe `timer_safe_point_injection_setting`: unset, `1` and empty are on; `0` is
  off.

### Verification

Linux x64 core probe **30 of 30**, Win32 x86 **28 of 28**, full Win32 build with no
errors. The change is inside `#if defined(__x86_64__)`, so the Win32 x86 binary's
behavior does not change.

Linux, 30 s: by default no faults, tick counter 438 / 2,079 / 2,749 / 4,848 at
10 / 17 / 20 / 29 s, safe points trapped/injected/deferred 3,073 / 2,952 / 121.
With `=0`: no faults, 412 / 1,752 / 1,754 / 1,754 — the clock stops from 17 s as in
Task 715 — and 3,923 / 0 / 3,923.

180 s on both hosts: no faults on either. The tick counter resets at 33 s
(6,406→317) and 110 s (18,566→470) on Win32, and at 42 s (8,064→387) and 126 s
(19,821→333) on Linux — intervals of 77 and 84 seconds. Linux drew 9,994 frames with
15,660 of 15,779 safe-point injections. The counter is one the game winds back each
cycle, so both hosts reset it at the same point; Linux starts later because its
asset decode is slower (Task 715), and the intervals differ by 9%.

### Next

* The 9% longer interval, and the larger count before the first reset (8,064 against
  6,406), may mean Linux lingers longer in some scene. Lining up per-scene times on
  both hosts is the next comparison.
* The two hosts still return different values when the guest reads a segment
  register (Task 717).
