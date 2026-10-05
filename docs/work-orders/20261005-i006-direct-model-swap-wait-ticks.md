# #6 작업 지시: direct 모델에서도 swap 대기 중 타이머 tick 전달

Issue: [#6](https://github.com/nworkers/rePIU/issues/6) · 설계: [20261005-i006](../design/20261005-i006-direct-model-swap-wait-ticks.md)

## 절차

1. `glide_gate_interrupt_exit.{h,cpp}`: 인코딩, 프레임 재배치, 출구 코드 페이지. `repiu_exe`에 추가.
2. `ResolveAotDbtGlideGateFrame`: 주입이 일어난 경우 재배치하고 반환(캐시 목적지 조회를 하지 않음).
3. `InjectsTicksDuringSwapWait()` direct 모델 true, `GlideSwapWaitTicksEnabled()`의 출구 코드 확인.
4. probe `glide_gate_interrupt_exit`(core probe, Win32 AOT probe).
5. `ARCHITECTURE.md`, `linux-port-frontier.md`, 작업 로그.
6. Linux i386 Release·x64 Debug 빌드, core probe, 실기 검증(설계의 검증 3, 4).

## 완료 조건

probe가 통과하고, Linux i386에서 창을 최소화해도 tick이 대량으로 버려지지 않음이 측정으로 남으며, 정상 실행과 x64에 회귀가
없습니다. Win32는 CI 빌드 결과와 사용자 확인 항목으로 기록합니다.

---

# #6 Work Order: Timer Ticks During the Swap Wait on the Direct Model Too

Issue: [#6](https://github.com/nworkers/rePIU/issues/6) · Design: [20261005-i006](../design/20261005-i006-direct-model-swap-wait-ticks.md)

## Steps

1. `glide_gate_interrupt_exit.{h,cpp}`: the encoding, the frame rearrangement and the exit code page, added to `repiu_exe`.
2. `ResolveAotDbtGlideGateFrame`: when an injection happened, rearrange and return (no cache target lookup).
3. `InjectsTicksDuringSwapWait()` true on the direct model; the exit code check in `GlideSwapWaitTicksEnabled()`.
4. The `glide_gate_interrupt_exit` probe (core probe and the Win32 AOT probe).
5. `ARCHITECTURE.md`, `linux-port-frontier.md`, the work log.
6. Linux i386 Release and x64 Debug builds, the core probe, and the real-hardware checks (3 and 4 of the design).

## Done when

The probe passes; measurements show that on Linux i386 minimising the window no longer drops ticks in bulk; and normal runs
and x64 show no regression. Win32 is recorded as the CI build result and an item for the user to check.
