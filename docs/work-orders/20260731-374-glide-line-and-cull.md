# 작업 지시: Glide 선 그리기와 양방향 cull / Work order: Glide line drawing and bidirectional culling

Task 374. 설계: [20260731-374](../design/20260731-374-glide-line-and-cull.md)

## 한국어

### 목표와 단계

`gameplay-capture.log`의 `grDrawLine` 미구현 11,024건과 `grCullMode(1)` 미지원
1건을 실제 HLE/OpenGL 동작으로 교체합니다.

1. 플랫폼 공용 `GlideDrawVertex`와 60바이트 producer decoder를 전용 파일로 분리합니다.
2. triangle boundary를 공용 decoder로 옮기고 line의 두 guest 정점을 검증·decode합니다.
3. backend line/triangle 제출을 합치고 line은 1픽셀 `GL_LINES`로 그립니다.
4. cull mode 0/1/2를 origin-aware OpenGL face 상태로 변환합니다.
5. render probe, architecture, analysis, KB, 작업 로그를 갱신합니다.
6. Win32 x86 Debug/Release 전체 빌드와 probe를 실행합니다.

### 완료 조건과 비범위

정상 `grDrawLine`은 미구현 기록 없이 `ret 8` 계약을 지키고, cull 0/1/2는 모두
성공하며 다른 값만 거부해야 합니다. 정점 decoder와 cull 변환 probe, 두 구성이 모두
통과해야 합니다. live 재검증 가능 여부를 로그에 남깁니다.

캡처에서 호출되지 않은 point, polygon, AA primitive는 별도 작업입니다. 원본 실행 파일과
게임 로직은 변경하지 않습니다.

## English

Replace the capture's 11,024 unimplemented `grDrawLine` calls and one rejected
`grCullMode(1)` call. Extract the shared 60-byte vertex decoder, migrate the
triangle boundary, validate and decode both line endpoints, share backend
primitive submission, and translate cull modes 0 through 2 by origin. Update
the probe and required documents, then build both Win32 x86 configurations.

Done means line rendering preserves `ret 8` without an unimplemented record,
all valid cull modes succeed, invalid modes fail, probes and builds pass, and
live verification limits are recorded. Unobserved point, polygon, and AA
primitives remain out of scope.
