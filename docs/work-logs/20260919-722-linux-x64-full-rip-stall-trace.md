# Task 722 작업 로그 — Linux x64 full RIP 정지 trace

설계: [20260919-722](../design/20260919-722-linux-x64-full-rip-stall-trace.md) ·
작업 지시: [20260919-722](../work-orders/20260919-722-linux-x64-full-rip-stall-trace.md) ·
frontier: [linux-port-frontier](../analysis/linux-port-frontier.md) ·
선행: [20260919-721](20260919-721-linux-x64-stall-position-census.md)

## 수행 결과

Linux signal `ucontext_t`에서 full native RIP를 읽는 read-only adapter를 추가했습니다.
native sampler는 Linux x64 opt-in 경로에서 context callback을 통해 이를 별도 sample field에
저장하고 false를 반환하므로, 32-bit guest ABI 및 native-context no-write-back 계약을
바꾸지 않습니다. `REPIU_LINUX_X64_NATIVE_SAMPLE_TRACE=1`일 때만 poll thread가 성공
census 뒤 시간순 trace를 출력하며 signal handler 안에서 formatting 또는 I/O를 하지 않습니다.

## 관찰

500ms trace `pumpit2a`는 timeout immediate-exit까지 완료했습니다(capture 49, distinct 41,
overflow 0, failure 0). 6.5–9초 창에는 shared-library RIP, cache-mapped `0x0103F139`,
`GlideOpenGlBackend::SpinForRendezvousHint` 인접 RIP가 섞였습니다. 이는 renderer와
host-command rendezvous/LFB 영역을 다음 frontier로 좁히지만 원인을 입증하지는 않습니다.

100ms trace는 약 25초에 fault를 노출했지만, 같은 100ms census에서 trace를 끄면 246 capture,
152 distinct, overflow 0, failure 0으로 fault 없이 timeout 종료했습니다. 500ms trace도
정상 종료했으므로 이 결과는 고빈도 출력의 timing perturbation으로 기록하며 callback 회귀로
해석하지 않습니다.

## 검증

- Linux x64 Debug `repiu_core_probe` 30/30 성공
- Win32 x86 Debug 전체 빌드 성공 및 `repiu_core_probe` 28/28 성공
- Linux x64 500ms trace: full RIP, monotonic elapsed time, capture failure 0 확인

---

## English

Design: [20260919-722](../design/20260919-722-linux-x64-full-rip-stall-trace.md) ·
Work order: [20260919-722](../work-orders/20260919-722-linux-x64-full-rip-stall-trace.md) ·
Frontier: [linux-port-frontier](../analysis/linux-port-frontier.md) ·
Predecessor: [20260919-721](20260919-721-linux-x64-stall-position-census.md)

### Result

Added a read-only adapter that reads full native RIP from Linux signal `ucontext_t`.
The Linux x64 opt-in sampler uses a context callback to retain it in a separate sample
field and returns false, preserving both the 32-bit guest ABI and native-context
no-write-back contract. Only `REPIU_LINUX_X64_NATIVE_SAMPLE_TRACE=1` writes a
time-ordered post-census trace on the poll thread; the signal handler does no formatting
or I/O.

### Observation

A 500ms traced `pumpit2a` completed through timeout immediate-exit (49 captures, 41
distinct positions, zero overflow, zero failures). The 6.5–9-second window mixed
shared-library RIPs, cache-mapped `0x0103F139`, and RIP adjacent to
`GlideOpenGlBackend::SpinForRendezvousHint`. It narrows the next frontier to renderer
and host-command rendezvous/LFB work without proving causality.

A 100ms trace exposed a fault at about 25 seconds, while the same 100ms census without
trace exited fault-free at timeout with 246 captures, 152 distinct positions, zero
overflow, and zero failures. The 500ms trace also completed, so this is recorded as
high-rate-output timing perturbation, not a callback regression.

### Verification

- Linux x64 Debug `repiu_core_probe`: 30/30 passed.
- Full Win32 x86 Debug build and `repiu_core_probe`: 28/28 passed.
- Linux x64 500ms trace: verified full RIP, monotonic elapsed time, and zero capture failures.
