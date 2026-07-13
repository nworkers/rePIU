# Glide Drawing HLE Stubs 작업 지시서
# Glide Drawing HLE Stubs Work Order

## 1. 작업 개요 (Task Overview)
* **목적:** Voodoo 3Dfx 코어 그리기 API 5종(`_GRDRAWPOINT@4`, `_GRDRAWTRIANGLE@12`, `_GRDRAWPLANARPOLYGON@12`, `_GRDRAWPLANARPOLYGONVERTEXLIST@8`, `_GRDRAWPOLYGON@12`)의 HLE signature 메타데이터를 추가하고 디스패처 스택 팝 처리를 이식하여 렌더링 루틴 상의 예외 크래시를 전방위 예방하고 `grDrawTriangle` 호출까지 안전하게 진행합니다.
* **관련 문서:** `docs/design/20260714-glide-drawing-stubs-hle.md`

* **Goal:** Register HLE signatures and dispatcher stack cleanup code for five core drawing APIs: `_GRDRAWPOINT@4`, `_GRDRAWTRIANGLE@12`, `_GRDRAWPLANARPOLYGON@12`, `_GRDRAWPLANARPOLYGONVERTEXLIST@8`, and `_GRDRAWPOLYGON@12`. This bypasses rendering gate crashes and guides execution to `grDrawTriangle`.
* **References:** `docs/design/20260714-glide-drawing-stubs-hle.md`

---

## 2. 세부 구현 사항 (Detailed Tasks)

### 1) glide_hle.cpp 수정
* `kObservedSignatures` 배열 크기를 `31` 로 설정하고, 5종의 그리기 API 메타데이터를 삽입합니다.

* Modify `kObservedSignatures` capacity to `31` and insert signatures in `src/hle/glide_hle.cpp`.

### 2) execution_trampoline.cpp 수정
* `DispatchWin32GlideHle` 함수 꼬리에 각 API 명칭 매칭과 스택 팝(8, 12, 16바이트) stub 핸들러 코드를 추가합니다.

* Inject 5 dispatch branches with proper stack cleanup size (8, 12, or 16 bytes) at the end of `DispatchWin32GlideHle` inside `src/platform/win32/execution_trampoline.cpp`.

---

## 3. 검증 방법 (Verification Procedure)
* `win32_x86_debug` 빌드를 다시 실행하여 에러 없이 성공적으로 링크됨을 확인합니다.
* `repiu_supervisor_win32.exe`를 사용하여 에뮬레이터를 구동하고, `glide_ordinal=73` (`grDrawTriangle`)을 비롯한 그리기 게이트 호출이 터지며 무한 루프 상태가 유지되는지 혹은 한층 더 깊이 전진하는지 콘솔 로그를 파싱해 실시간 확인합니다.

* Compile and verify the build links successfully.
* Run using `repiu_supervisor_win32.exe`. Ensure drawing gates (e.g., `glide_ordinal=73`) execute without crashing, verifying execution telemetry logs.
