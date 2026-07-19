# Task 248 작업 지시: Glide 프레임 루프 게이트(BufferClear/Swap/NumPending) ABI 보존 처리

## 배경

Task 247 검증 구동에서 `_GRBUFFERCLEAR@12`가 시그니처 부재로 미처리(2회)되어
Task 246이 확정한 미처리 게이트 → 스택 스캔 복구 → stdcall 프레임 누수 경로로
`ret 0xC` epilogue AV가 발생했다(게이트 진입 61→73으로 전진한 새 frontier).
grBufferClear는 grBufferSwap/grBufferNumPending과 함께 Glide 프레임 루프의 핵심
3종이므로 함께 관측 ABI를 보존한다.

## 작업 항목

1. `kObservedSignatures`에 `_GRBUFFERCLEAR@12`(void), `_GRBUFFERSWAP@4`(void),
   `_GRBUFFERNUMPENDING@0`(FxU32) 추가.
2. `HandleGlideGateBoundary`에 핸들러 추가 — 기존 렌더링 경계 정책(그리기 no-op)과
   동일하게 이미지 충실도는 후속 렌더 backend 과제로 두고 stdcall ABI만 보존:
   - BufferClear: no-op, `Esp += 4*4`.
   - BufferSwap: no-op(게이트 공통 PumpEvents 활용), `Esp += 2*4`.
   - BufferNumPending: `EAX=0`, `Esp += 1*4`.
3. 빌드 후 `aot-dynamic` 180초 검증: 해당 reject 소멸, 진행 지속, 다음 frontier 기록.

# Task 248 Work Order: ABI-Preserving Glide Frame-Loop Gates

`_GRBUFFERCLEAR@12` was rejected for a missing signature and triggered the
confirmed unhandled-gate frame-leak path (`ret 0xC` epilogue AV) after Task 247
advanced the gate traffic 61→73. Add observed signatures and ABI-preserving
handlers for grBufferClear/grBufferSwap/grBufferNumPending (rendering fidelity
stays with the future render-backend work), then verify with a 180-second
`aot-dynamic` run.
