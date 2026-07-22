# 작업 지시: AOT 경계 이탈 사유별 카운터
# Work Order: Per-Reason Counters for AOT Boundary Exits

**대상 설계 (Design):** `docs/design/20260722-262-aot-boundary-exit-reason-counters.md`
**Task:** 262 — 다음 단계 1번 (이탈 사유별 카운터 추가) / Next step 1

## 목표 (Goal)

`aot-dynamic`의 초당 약 1,400회 경계 이탈이 어느 사유(반환/간접/직접/조건/기타)에
몰려 있는지 관측 가능하게 한다. 게스트 실행 동작은 바꾸지 않는다.

Make the ~1,400/s AOT boundary exits observable by reason (return / indirect /
direct / conditional / other) without changing guest execution behavior.

## 변경 항목 (Change list)

1. **신규** `src/platform/win32/aot/aot_boundary_reason.h` — `enum class
   AotBoundaryReason` + `ClassifyAotBoundaryInstruction` 선언 (host-neutral).
2. **신규** `src/platform/win32/aot/aot_boundary_reason.cpp` — 선두 opcode 기반
   순수 분류기 구현.
3. `CMakeLists.txt` — 새 소스를 해당 라이브러리 타깃에 추가.
4. `src/platform/win32/execution/thread_context.h` — 사유별 atomic 5개 추가.
5. `src/platform/win32/aot/aot_runtime_dispatch.h` / `.cpp` —
   `BumpAotBoundaryReason` 추가, `BumpAotBoundaryCount` 호출 지점에서
   `IsGuestRangeReadable`로 게스트 바이트 확보 후 분류·집계.
6. `include/repiu/platform/win32/live_telemetry.h` — 사유별 volatile 필드 5개,
   버전 16 → 17.
7. `src/platform/win32/telemetry/live_telemetry_snapshot.cpp` — 요약 복사.
8. `include/repiu/platform/win32/execution_trampoline.h` — 결과 구조체 필드 5개.
9. `src/host/win32/main.cpp` — 정상 종료 요약 한 줄 출력.
10. `src/host/win32/supervisor_main.cpp` — 주기 외부 덤프 한 줄 출력.

## 검증 (Verification)

* 분류기(`aot_boundary_reason.cpp`)를 대표 opcode 테이블 단위 테스트와 함께 Linux
  `g++`로 단독 컴파일·실행. 합 = 총 이탈 수 불변식 확인.
* Win32 전 경로 빌드 및 `pumpit1` 런타임 재구동은 이 환경에서 불가 → 사유와
  후속 실행 절차를 작업 로그에 기록.

## 완료 조건 (Done)

* 사유별 카운터가 스레드·공유 텔레메트리·정상 종료 요약·supervisor 덤프에서 모두
  노출된다.
* 분류기 단위 검증 통과.
* 설계·작업 지시·작업 로그·frontier 상태 갱신 완료.
