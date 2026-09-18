# Task 718 설계 — Linux x64 safe point 틱 주입을 기본값으로

## 목적

Linux x64는 렌더 단계에 들어가면 게임 시계가 멈춘다(Task 715). 렌더 단계의 틱은
거의 전부 AOT safe point로 들어가는데(Win32 5,667/5,729), Linux에서는 그 주입이
opt-in이기 때문이다. opt-in으로 둔 이유였던 두 크래시는 Task 716(`CS:` boundary)과
Task 717(ES fold)에서 없어졌다. 이제 기본값으로 켠다.

## 설계

`REPIU_LINUX_X64_SAFE_POINT_INJECTION`의 의미를 바꾼다.

| 값 | 이전 | 이후 |
|---|---|---|
| 없음 | off | **on** |
| `1` 등 | on | on |
| `0` | **on**(존재만 검사) | **off** |

* 해석은 `runtime::ResolveTimerSafePointInjection`(헤더 전용)에 두고 probe로 검증한다.
* `0`만 off로 읽는다. 이전 실행들이 쓰던 `=1`은 계속 on이다.
* 변경은 `__x86_64__` 경로 안이라 Win32 x86 바이너리는 바뀌지 않는다.

## 검증

* probe: 없음·`1`·빈 문자열은 on, `0`은 off
* Linux 기본값 30초: 시계가 흐름, 폴트 0
* Linux `=0` 30초: Task 715의 동작(17초부터 정지)
* Linux 기본값 180초: 폴트 0, Win32 180초와 게임 주기 비교
* Win32: core probe

---

## English

### Purpose

The Linux x64 game clock stops once rendering starts (Task 715): render-phase ticks
enter almost entirely at AOT safe points (5,667 of 5,729 on Win32), and on Linux
that injection is opt-in. The two crashes that kept it opt-in are gone after Task
716 (the `CS:` boundary) and Task 717 (the ES fold). This task turns it on by
default.

### Design

The meaning of `REPIU_LINUX_X64_SAFE_POINT_INJECTION` changes: unset was off and
is now **on**; `1` and similar stay on; `0` was **on** (the variable was tested for
presence) and is now **off**. The reading lives in the header-only
`runtime::ResolveTimerSafePointInjection` and is probed. Only `0` reads as off, so
the `=1` earlier runs used keeps meaning on. The change sits inside the
`__x86_64__` path, so the Win32 x86 binary does not change.

### Verification

* Probe: unset, `1` and empty are on; `0` is off.
* Linux default, 30 s: the clock runs, no faults.
* Linux `=0`, 30 s: Task 715's behavior (stops from 17 s).
* Linux default, 180 s: no faults; the game's cycle compared with a 180-second
  Win32 run.
* Win32: core probe.
