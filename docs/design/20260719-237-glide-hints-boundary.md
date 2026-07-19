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
