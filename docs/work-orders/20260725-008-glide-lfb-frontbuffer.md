# Work Order: Glide LFB Frontbuffer Lock (20260725-008)

## 1. 목적 (Purpose)

Glide의 `grLfbLock` 호출 시 로더가 멈추거나 화면이 갱신되지 않는 문제를 해결한다.

## 2. 작업 내용 (Tasks)

1. `linexe_glide_boundary.cpp`의 `_GRLFBLOCK@24` 핸들러에서 `kGlideBufferFrontBuffer`도 허용하도록 수정한다.
2. `linexe_glide_boundary.cpp`의 `_GRLFBUNLOCK@8` 핸들러에서 FRONTBUFFER에 대한 Lock이 해제될 경우, `BufferSwap(1)`을 호출하여 즉시 화면에 갱신되도록 처리한다.

## 3. 검증 전략 (Verification)

- CMake 빌드가 성공하는지 확인한다.
- `grLfbLock`에서 로더가 무한 루프에 빠지지 않고, 화면 갱신이 올바르게 이루어지는지 사용자가 검증한다.
