# 20260718-233 작업 로그 — Phase 1 증분 12 (디렉토리 그룹화, 최종)

관련: [작업 지시서](../work-orders/20260718-233-execution-trampoline-decomposition-order.md) · [증분11 로그](20260718-233-execution-trampoline-phase1-increment11-log.md)

## 한 일 (Phase 1 마무리)

증분 1~11에서 분리한 모듈들을 서브시스템별 디렉토리로 그룹화(결정: 서브시스템별 세분화, 마지막 일괄 이동):

```
src/platform/win32/
  execution/  execution_trampoline.cpp, thread_context.h, execution_internal.h, win32_thread_api.h
  exception/  exception_rescue_win32.{h,cpp}
  io/         port_io_emulator.{h,cpp}
  dos/        dos_int21_services.{h,cpp}, dpmi_mscdex_services.{h,cpp}
  cpu_emul/   instruction_emulation.{h,cpp}, guest_memory_access.{h,cpp}
  aot/        aot_runtime_dispatch.{h,cpp}
  boundary/   linexe_glide_boundary.{h,cpp}
  telemetry/  live_telemetry_snapshot.{h,cpp}
```

- `git mv`로 파일 이동, `CMakeLists.txt` 소스 경로 갱신.
- 모듈 간 `#include "foo.h"` 상대 include문은 **그대로 유지**하고, `target_include_directories(repiu_exe PRIVATE ...)`에 `src/platform/win32` + 8개 서브디렉토리를 추가해 짧은 이름으로 해석되게 함(최소 편집·저위험).
- 기존 win32 파일(aot_code_cache_win32, glide_opengl_*, native_* 등)은 이번 분해 대상이 아니므로 win32/ 잔류.

## 검증
```
cmake --build build/win32_x86_dpmi --config Debug --target repiu_loader_win32  # green (전체 재컴파일·링크)
```

## Phase 1 요약
`execution_trampoline.cpp` **12,117 → 3,196줄 (74% 감소)**. 12개 하위 시스템 모듈로 분해. 트램폴린에는 VEH 디스패치 코어 + 실행 드라이버(RunWin32ExecutionThread/Attempt*) + 공유 substrate만 잔류(통합 지점). 각 증분 동작 보존(순수 코드 이동)·빌드 검증·독립 커밋. Phase 2(GuestCpuFrame seam)·3(서비스 의미론 중립화)은 두 번째 플랫폼 백엔드 필요 시 착수.
