# Task 718 작업 지시 — Linux x64 safe point 틱 주입을 기본값으로

설계: [20260919-718](../design/20260919-718-default-safe-point-injection.md)

## 범위

Linux x64 safe point 틱 주입을 기본값 on으로 바꾸고, `REPIU_LINUX_X64_SAFE_POINT_INJECTION=0`
으로만 끄게 한다.

## 단계

1. `ResolveTimerSafePointInjection`과 trampoline 연결
2. probe
3. 두 host 빌드·core probe
4. Linux 기본값·`=0` 30초, 기본값 180초, Win32 180초
5. 작업 로그, frontier

## 검증

* Linux x64 core probe 30/30, Win32 28/28
* 기본값: 폴트 0, 시계 흐름 / `=0`: 이전 동작

---

## English

Design: [20260919-718](../design/20260919-718-default-safe-point-injection.md)

### Scope

Make Linux x64 safe-point tick injection on by default, turned off only by
`REPIU_LINUX_X64_SAFE_POINT_INJECTION=0`.

### Steps

1. `ResolveTimerSafePointInjection`, wired into the trampoline.
2. Probe.
3. Builds and core probes on both hosts.
4. Linux default and `=0` for 30 s, Linux default for 180 s, Win32 for 180 s.
5. Work log and frontier.

### Verification

* Linux x64 core probe 30 of 30, Win32 28 of 28.
* Default: no faults, the clock runs. `=0`: the earlier behavior.
