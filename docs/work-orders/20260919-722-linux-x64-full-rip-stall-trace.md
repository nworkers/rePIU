# Task 722 작업 지시 — Linux x64 full RIP 정지 trace

설계: [20260919-722](../design/20260919-722-linux-x64-full-rip-stall-trace.md)

## 범위

Linux x64 opt-in census가 signal context의 full native RIP와 sample elapsed time을 기록하게
합니다. guest ABI context, 기본 실행, Win32 x86은 변경하지 않습니다.

## 단계

1. Linux guest-context adapter에 full RIP read helper를 추가합니다.
2. native sampler의 context callback으로 full RIP를 별도 sample field에 기록합니다.
3. opt-in trace line을 poll thread에서 출력합니다.
4. 빌드·probe·실제 `pumpit2a` 관찰로 확인하고 analysis/work log를 갱신합니다.

## English

Design: [20260919-722](../design/20260919-722-linux-x64-full-rip-stall-trace.md)

### Scope

Record full native RIP and sample elapsed time for Linux x64 opt-in census, without
changing the guest ABI context, default execution, or Win32 x86.

### Steps

Add the Linux full-RIP helper; record it through the native sampler context callback;
emit an opt-in poll-thread trace line; build, probe, observe `pumpit2a`, and update
analysis/work log.
