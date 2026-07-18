# 20260718-233 작업 로그 — Phase 1 증분 8 (MSCDEX + DPMI + 마우스)

관련: [작업 지시서](../work-orders/20260718-233-execution-trampoline-decomposition-order.md) · [증분7 로그](20260718-233-execution-trampoline-phase1-increment7-log.md)

## 한 일

MSCDEX(패킷·MSF/LBA·ioctl·request), DPMI INT 31h, 마우스 INT 33h 핸들러를 `dpmi_mscdex_services.{h,cpp}`로 분리(CMake 등록). 증분 7에서 트램폴린으로 재배치했던 `HandleMscdexRequest`도 이 모듈로 이동(execution_internal.h 선언 제거, dos_int21_services.cpp에 새 헤더 include 추가).

## 경계 처리 (빌드가 잡아냄)
- `ResolveSegmentLinearRange`(MSCDEX+세그먼트 공유, fwd 498/def 3941)를 외부 링크 승격(execution_internal.h). 첫 grep이 `Resolve*` 제외 패턴으로 놓쳤으나 빌드(C3861)가 잡음.
- `ResolveMscdexBuffer` 기본 인자(`resolve_kind = nullptr`)는 헤더 선언에만 두고 .cpp 정의에서 제거(C2572).

`execution_trampoline.cpp`: 8,686 → 8,082줄.

## 검증
```
cmake --build build/win32_x86_dpmi --config Debug --target repiu_loader_win32  # green
```

## 다음
증분 9: 명령어 에뮬레이션(세그먼트 명령 + traced 메모리 명령 + 레지스터/플래그/디코드 헬퍼) — 최대 클러스터.
