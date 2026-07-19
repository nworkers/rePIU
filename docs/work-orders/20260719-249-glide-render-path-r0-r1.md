# 2026-07-19 Task 249 Work Order: Glide Render Path Completion (R0 & R1)

## 개요 / Overview
설계 문서 `docs/design/20260719-249-glide-render-path-completion.md`에 따라 렌더 경로 보완의 첫 단계인 R0(게이트 안전망)와 R1(프레임 제시)을 구현한다.

This work order implements Phase R0 (Gate Safety Net) and R1 (Frame Presentation) based on the design document to systematically resolve the black screen issue and prevent unhandled gate crashes.

## 구현 목표 / Implementation Goals

### R0. 게이트 안전망 구축 (Gate Safety Net)
1. **시그니처 카탈로그 확장**: `src/hle/glide_hle.cpp`의 `kObservedSignatures` 배열을 확장하여 PIU.EXE가 참조하는 전체 97개의 Glide API 시그니처를 등록한다.
2. **기본 핸들러(Default Handler) 추가**: `linexe_glide_boundary.cpp`의 `HandleGlideGateBoundary` 끝부분에 카탈로그 기반의 기본 핸들러를 추가한다. 미처리된 게이트가 호출될 경우, 카탈로그의 `argument_byte_count`를 참조하여 stdcall 스택 정리를 수행하고, 반환 타입에 맞는 안전한 기본값(EAX=0 등)을 설정한 후 정상 반환시킨다. 이를 통해 프레임 누수 및 크래시 사슬을 방지한다.
3. **거부 텔레메트리 보완**: 백엔드 호출이 실패하여 `return false`를 수행하는 경로들을 `reject_gate` 호출로 교체하여 미계측 상태로 실패하는 것을 막는다.

### R1. 프레임 제시 경로 (Frame Presentation)
1. **백엔드 인터페이스 추가**: `GlideOpenGlBackend`에 `BufferClear(color, alpha, depth)`와 `BufferSwap(swap_interval)` 메서드를 추가한다.
2. **백엔드 구현**: `BufferClear`는 `glClearColor`, `glClearDepth`, `glClear`를 사용해 구현하며, `BufferSwap`은 `SwapBuffers`를 호출한다.
3. **게이트 연동**: `_GRBUFFERCLEAR@12`와 `_GRBUFFERSWAP@4` 게이트 핸들러가 no-op 대신 백엔드의 메서드를 호출하도록 연결한다.

## 검증 계획 / Verification Plan
1. `pumpit1` aot-dynamic 구동을 통해 크래시 없이 600초 이상 실행되는지 확인한다.
2. 추가된 R0 기본 핸들러가 새로 관측되는 미구현 게이트들을 안전하게 넘기고 로그/텔레메트리를 남기는지 확인한다.
3. R1 적용 후 `BufferClear`에 임시로 식별 가능한 색상(예: 자홍색)을 설정하여, 윈도우 스왑이 실제로 일어나고 화면이 갱신되는지 육안으로 확인한다.
4. `docs/work-logs/20260719-249-glide-r0-r1-log.md`를 작성하여 결과를 기록한다.
