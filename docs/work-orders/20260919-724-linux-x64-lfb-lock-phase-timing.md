# Task 724 작업 지시 — Linux x64 LFB lock 단계별 timing

설계: [20260919-724](../design/20260919-724-linux-x64-lfb-lock-phase-timing.md)

## 범위

`grLfbLock` staging seed의 readback 및 RGBA8→565 encode 시간을 기본 OFF 계측으로
추가합니다. Glide LFB의 의미와 Win32 x86 동작을 바꾸지 않습니다.

## 단계

1. 전용 telemetry header/source 및 environment policy를 추가합니다.
2. `ThreadContext`에 profile을 소유시키고 lock handler에서 단계별 시간을 기록합니다.
3. final report snapshot과 summary를 연결합니다.
4. Linux x64/Win32 x86 build·core probe, Win32 x86 `repiu_aot_probe --glide-lfb-timing`, Linux bounded
   observation을 수행합니다.
5. analysis와 작업 로그에 확정 결과 및 남은 제한을 기록합니다.

## English

Design: [20260919-724](../design/20260919-724-linux-x64-lfb-lock-phase-timing.md)

### Scope

Add default-off timing for readback and RGBA8-to-565 encoding in `grLfbLock` staging
seed, without changing Glide LFB semantics or Win32 x86 behavior.

### Steps

Add dedicated telemetry header/source and environment policy; own the profile in
`ThreadContext` and record phases in the lock handler; connect final-report snapshot
and summary; run Linux x64/Win32 x86 build/core probes, Windows x86 `repiu_aot_probe --glide-lfb-timing`,
and bounded Linux observation;
document confirmed results and remaining limits.
