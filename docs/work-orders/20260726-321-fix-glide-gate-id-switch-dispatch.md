# 20260726-321 작업 지시: GlideGateId 기반 안전한 O(1) Switch Dispatch 구현 / Work order: Implement Safe GlideGateId Switch Dispatch

설계: [20260726-321-fix-glide-gate-id-switch-dispatch.md](../design/20260726-321-fix-glide-gate-id-switch-dispatch.md)

## 한국어

### 목표

`glide_export->ordinal` 대신 `GlideGateId` 고유 Enum을 통한 `switch (glide_export->gate_id)` 점프 테이블을 구축하여 `_GRSSTWINOPEN@28` 미매칭 오류를 근본적으로 해결하고, 문자열 비교 없는 고성능 O(1) 바운더리를 확립한다.

---

### 작업 내용

1. `include/repiu/hle/glide_hle.h`
   - `enum class GlideGateId : std::uint16_t` 선언 및 99개 Glide 함수 ID 정의 (`kUnknown = 0`, `kGrGlideInit`, `kGrSstWinOpen` 등).
   - `GlideExportGate` 구조체에 `GlideGateId gate_id` 필드 추가.
   - `GlideGateId ResolveGlideGateId(std::string_view name)` 도우미 함수 선언.

2. `src/hle/glide_hle.cpp`
   - `ResolveGlideGateId` 구현 (이름 기반 ID 반환).
   - `BuildGlideGatePlan`에서 각 `GlideExportGate` 생성 시 `gate_id` 할당.

3. `src/platform/win32/boundary/linexe_glide_boundary.cpp`
   - `glide_ordinal` 상수를 `GlideGateId`로 대체 (`namespace go = repiu::hle;`).
   - `switch (glide_export->gate_id)` 로 점프 테이블 변경.

4. 검증
   - Debug 빌드 (`cmake --build build --config Debug`).
   - 10초 스모크 런타임 테스트 실행하여 `_GRSSTWINOPEN@28` 정상 수용 및 `fxMesa` 컨텍스트 성공 확인.

---

## English

### Tasks

1. Define `enum class GlideGateId` in `glide_hle.h` and add `gate_id` field to `GlideExportGate`.
2. Implement `ResolveGlideGateId` in `glide_hle.cpp` and populate `gate_id` during plan creation.
3. Switch dispatch in `linexe_glide_boundary.cpp` to use `switch (glide_export->gate_id)`.
4. Build Debug and verify via 10s smoke test.
