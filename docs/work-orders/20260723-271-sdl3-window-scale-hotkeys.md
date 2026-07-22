# SDL3 창 배율 단축키 작업 지시

## 한국어

### 작업 범위

1. `GlideOpenGlBackend`에 기본 2배 창 배율 상태와 1–4배 적용 helper를 추가합니다.
2. SDL key-down event에서 반복 입력을 제외한 `Alt+1..4`를 처리합니다.
3. 초기 창과 단축키 전환 시 논리 해상도에 배율을 곱한 host window 크기를 적용합니다.
4. SDL resize/pixel-size event에서 OpenGL viewport와 scissor를 drawable 전체로 갱신합니다.
5. 확대 framebuffer의 LFB readback을 논리 해상도로 축소합니다.
6. 아키텍처와 작업 로그를 갱신하고 빌드·실행 검증 후 커밋합니다.

### 완료 조건

- 기본 창 크기가 논리 해상도의 2배
- `Alt+1..4`가 각각 정확한 정수 배율 적용
- 게스트 논리 해상도와 LFB 출력 크기 유지
- Win32 x86 Debug 빌드 성공

## English

### Scope

1. Add a default 2× scale state and 1×–4× application helper to `GlideOpenGlBackend`.
2. Handle non-repeating `Alt+1..4` SDL key-down events.
3. Size the initial and hotkey-selected host window to logical dimensions multiplied by the scale.
4. Update the OpenGL viewport and scissor to the complete drawable on SDL resize/pixel-size events.
5. Downsample enlarged framebuffer LFB readback to logical dimensions.
6. Update architecture and work-log documentation, verify build/runtime behavior, and commit.

### Completion criteria

- Default window size is 2× the logical resolution
- `Alt+1..4` applies the exact corresponding integer scale
- Guest logical resolution and LFB output dimensions are preserved
- Win32 x86 Debug build succeeds
