# Glide Hints 경계 설계 / Glide Hints Boundary Design

## 목적 / Purpose

`_GRHINTS@8`은 텍스처 초기화 뒤 새로 관측된 Glide 경계입니다. 원본 `PIU.EXE`의 호출 ABI를 보존하면서, 현재 렌더러가 힌트 최적화를 구현하지 않는다는 사실을 명시적으로 유지합니다.

`_GRHINTS@8` is the next observed Glide boundary after texture initialization. Preserve the original `PIU.EXE` ABI while explicitly retaining the fact that the current renderer does not implement hint optimizations.

## 정책 / Policy

- ABI는 `void grHints(GrHints_t type, FxU32 hintMask)`이며 8바이트 stdcall cleanup을 수행합니다.
- 호출 인자는 telemetry에 의해 기존 gate 관찰 경로로 남습니다.
- 힌트는 최적화 조언이므로, 검증된 렌더링 의미가 생길 때까지 platform-neutral 상태나 게임 로직을 변경하지 않는 no-op입니다.

- The ABI is `void grHints(GrHints_t type, FxU32 hintMask)` with eight-byte stdcall cleanup.
- Existing gate telemetry retains the arguments.
- Hints are optimization advice, so the call is a no-op without changing platform-neutral state or game logic until verified rendering semantics require it.
- An observed color-combine equation rejected only as unsupported by the current GLSL translator is retained in GlideLogicalState and returns normally; other backend failures remain fatal to the boundary.

## 2026-07-19 Task 247 확장 / Task 247 Extension

- Task 246 채증으로 `_GRALPHACOMBINE@20`의 미지원 GLSL 식 실패가 게이트 미처리로
  이어지고, 미처리 예외가 AOT 스택 스캔 복구를 거쳐 반환 주소로 ESP 미조정 점프해
  stdcall 24바이트를 누수시키는 것이 zero-EIP(0x0304ED35)의 근인으로 확정되었다.
- 따라서 color-combine에 이미 적용된 유지 정책을 alpha-combine에 동일 적용한다:
  GLSL 번역기가 "unsupported Glide alpha-combine equation"으로만 거부한 식은
  GlideAlphaCombineState로 유지하고 정상(stdcall 정리 포함) 반환한다. 그 외
  backend 실패는 기존대로 경계 실패로 남긴다.

- Task 246 evidence confirmed that an unsupported-equation failure of
  `_GRALPHACOMBINE@20` leaves the gate unhandled; the unhandled exception then
  reaches the AOT stack-scan recovery, which jumps to the return address without
  adjusting ESP, leaking the 24-byte stdcall frame — the root cause of the
  zero-EIP at `0x0304ED35`.
- The retain policy already applied to color combine therefore extends to alpha
  combine: an equation rejected only as "unsupported Glide alpha-combine
  equation" is retained in GlideAlphaCombineState and returns normally with the
  stdcall cleanup. Other backend failures remain fatal to the boundary.

- 2026-07-19 (Task 248 추가): 동일 유지 정책을 `_GRALPHABLENDFUNCTION@16`의
  "unsupported Glide alpha-blend function" 거부에도 적용한다(관측: ret=0x0304F49C
  진입 2회 미처리 → 프레임 누수 → ret 0xC epilogue AV). 프레임 루프 게이트
  `_GRBUFFERCLEAR@12`/`_GRBUFFERSWAP@4`/`_GRBUFFERNUMPENDING@0`은 그리기 계열과
  같은 렌더링 경계 no-op으로 ABI만 보존한다.
- 2026-07-19 (Task 248): The same retain policy extends to
  `_GRALPHABLENDFUNCTION@16` rejections with "unsupported Glide alpha-blend
  function" (observed: two unhandled entries at ret=0x0304F49C leaking the frame
  into a `ret 0xC` epilogue AV). The frame-loop gates grBufferClear/Swap/
  NumPending are ABI-preserving rendering-boundary no-ops like the draw calls.
