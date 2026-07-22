# SDL3 창 배율 단축키 작업 로그

## 한국어

### 결과

- SDL3 Glide 창의 기본 크기를 논리 해상도의 2배로 변경했습니다.
- `Alt+1`, `Alt+2`, `Alt+3`, `Alt+4`에 각각 1배, 2배, 3배, 4배 창 크기를 연결했습니다.
- SDL event pump를 메인 실행 대기 루프에서 지속 수행하여 Glide 호출이 드문 구간에도 단축키와 일반 resize가 반응하게 했습니다.
- resize/pixel-size event마다 OpenGL viewport와 full-window scissor를 drawable 전체로 갱신합니다.
- 확대된 framebuffer의 LFB readback은 drawable 전체를 읽고 논리 해상도로 최근접 축소합니다.
- 종료 시 배율을 기본 2배로 초기화하여 backend 재개방도 같은 기본값을 사용합니다.

### 검증

- `cmake --build build\win32_x86_debug --config Debug -- /m:1`: 성공
- 실제 `pumpit1` 실행에서 기본 `640×480 logical → 1280×960 host` 확인
- 실제 SDL 창을 대상으로 자동 키 입력 검증:
  - `Alt+1`: `640×480`
  - `Alt+2`: `1280×960`
  - `Alt+3`: `1920×1440`
  - `Alt+4`: `2560×1920`
- 검증 실행 중 `SDL_IsMainThread` assertion과 guest 교착 없음
- 저장소에 등록된 CTest는 없어 별도 단위 테스트는 실행되지 않음

### 구현 중 수정 사항

초기 패치에서 배율 크기 검증 블록이 `SDL_InitSubSystem` 조건과 중괄호 사이에 들어가 dummy fallback이 항상 실행되는 제어 흐름 오류를 실제 실행으로 발견했습니다. 조건문 구조를 복구한 뒤 재빌드하고 실제 창 생성 및 네 단축키를 다시 검증했습니다.

## English

### Result

- Changed the default SDL3 Glide window size to 2× the logical resolution.
- Bound `Alt+1`, `Alt+2`, `Alt+3`, and `Alt+4` to 1×, 2×, 3×, and 4× window sizes.
- Continuously pump SDL events from the main execution wait loop so hotkeys and ordinary resizing remain responsive even when Glide calls are sparse.
- Update the OpenGL viewport and full-window scissor to the complete drawable on resize/pixel-size events.
- LFB readback reads the complete enlarged framebuffer and nearest-neighbor downsamples it to logical dimensions.
- Reset the scale to the default 2× during shutdown so backend reopening uses the same default.

### Verification

- `cmake --build build\win32_x86_debug --config Debug -- /m:1`: passed
- Confirmed `640×480 logical → 1280×960 host` during a real `pumpit1` run
- Automated key input against the real SDL window confirmed:
  - `Alt+1`: `640×480`
  - `Alt+2`: `1280×960`
  - `Alt+3`: `1920×1440`
  - `Alt+4`: `2560×1920`
- No `SDL_IsMainThread` assertion or guest deadlock during verification
- No CTest tests are registered in the repository, so no separate unit tests ran

### Implementation correction

An initial patch placed scaled-size validation between the `SDL_InitSubSystem` condition and its braces, making the dummy fallback execute unconditionally. Runtime verification exposed the control-flow error. After restoring the condition structure, the project was rebuilt and real window creation plus all four hotkeys were verified again.
