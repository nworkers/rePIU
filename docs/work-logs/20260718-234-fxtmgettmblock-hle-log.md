# fxTMGetTMBlock() HLE 우회 작업 로그
# Work Log for fxTMGetTMBlock() HLE Bypass

---

## 1. 작업 개요 (Task Summary)
* **작업 브랜치 (Task Branch)**: `feature/hle-fxtmgettmblock`
* **목표 (Goal)**: `PIU.EXE` 초기 3dfx 그래픽스 초기화 시 호출되는 `grTexMinAddress` 및 `grTexMaxAddress` API의 스택 정리 오류(stdcall vs cdecl 정렬 불일치)를 수정하여, `fx Driver: internal error in fxTMGetTMBlock()` 에러 크래시를 우회하고 인게임 렌더 루프로 무사히 진행하도록 돕는다.
* **Goal**: Fix the stack misalignment bug (stdcall vs cdecl mismatch) in the HLE wrappers of `grTexMinAddress` and `grTexMaxAddress` triggered during graphics startup in `PIU.EXE`, bypassing the `fx Driver: internal error in fxTMGetTMBlock()` initialization failure and advancing execution into the main rendering loop.

---

## 2. 규명된 핵심 기술 정보 (Technical Findings)
* **원인**: standard Glide 2.x API `_GRTEXMINADDRESS@4`와 `_GRTEXMAXADDRESS@4`는 `stdcall`이지만 게스트 바이너리(`PIU.EXE`)는 이들을 `cdecl` 방식으로 호출하여 복귀 후 직접 `add esp, 4`를 처리하고 있었습니다.
* **오류 분석**: HLE 계층에서 해당 호출들에 대해 `ESP += 8` (복귀 주소 + 피호출자 인자 정리)을 일괄 적용하면서 게스트 측의 `add esp, 4`와 결합되어 총 `ESP += 12` (4바이트 초과 복구)가 일어나 스택 오염이 발생했습니다.
* **해결**: 복귀 스택 오프셋 정리를 `win32_context->Esp += 1U * sizeof(std::uint32_t)` (4바이트)로 줄여, 복귀 주소만 팝업하고 호출자 측에서 정상적으로 인자를 수거해 갈 수 있도록 정렬 규약을 유지했습니다.

---

## 3. 구현 내용 (Implemented Changes)
* **[linexe_glide_boundary.cpp](file:///e:/MYWORK/Projects/rePIU/src/platform/win32/boundary/linexe_glide_boundary.cpp)**:
  * `_GRTEXMINADDRESS@4` 및 `_GRTEXMAXADDRESS@4` 핸들러 복귀 시 스택 처리 코드를 `win32_context->Esp += 2U * sizeof(std::uint32_t)` -> `win32_context->Esp += 1U * sizeof(std::uint32_t)`로 수정.

---

## 4. 검증 결과 (Verification Results)
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe pumpit1` 실행:
  * **크래시 제거**: 기존에 출력되던 `fx Driver: internal error in fxTMGetTMBlock()` 에러 로그 및 강제 종료 현상이 완벽하게 사라짐.
  * **초기화 우회 및 전진**: 게스트 프로세스가 에러 없이 Glide 드라이버 초기화를 무사히 완수하고, 타임아웃(exit code: 3)이 감지될 때까지 렌더링 루프로 성공적으로 진입함을 검증함.
