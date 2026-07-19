# Glide R2 GrVertex 관측 작업 지시

## 목표

원본 PIU의 첫 `grDrawTriangle` 호출에서 실제 GrVertex 메모리 자료를 캡처해 R2 정점 렌더러의 근거를 확보한다.

## 범위

1. Win32 Glide gate context와 실행 attempt에 first-draw 관측 상태를 추가한다.
2. `_GRDRAWTRIANGLE@12`에서 세 포인터와 72바이트씩을 안전하게 캡처한다.
3. 실시간 및 종료 진단을 추가한다.
4. Win32 x86 debug 빌드를 수행한다.

## 제외

* 추정 레이아웃 기반 OpenGL draw
* texture/LFB 구현
* 게임 상태 머신 변경

# Glide R2 GrVertex Observation Work Order

## Objective

Capture actual GrVertex memory from PIU's first `grDrawTriangle` call to establish evidence for the R2 vertex renderer.

## Scope

1. Add first-draw observation state to the Win32 Glide gate context and execution attempt.
2. Safely capture three pointers and 72 bytes each at `_GRDRAWTRIANGLE@12`.
3. Add live and final diagnostics.
4. Build Win32 x86 debug.

## Exclusions

* OpenGL drawing based on an assumed layout
* Texture/LFB implementation
* Changes to the game state machine
