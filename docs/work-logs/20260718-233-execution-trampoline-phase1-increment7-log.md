# 20260718-233 작업 로그 — Phase 1 증분 7 (DOS INT 21h/2Fh 서비스)

관련: [작업 지시서](../work-orders/20260718-233-execution-trampoline-decomposition-order.md) · [증분6 로그](20260718-233-execution-trampoline-phase1-increment6-log.md)

## 한 일

DOS INT 21h/2Fh 서비스 핸들러 클러스터(998–2167, 순수 DOS)를 `dos_int21_services.{h,cpp}`로 분리(CMake 등록). 파일시스템 HLE는 이미 `repiu/hle/dos_file_system.h`, 공유 substrate는 증분 6에서 `execution_internal.h`로 승격돼 있어 경계가 깨끗했다.

- 유일한 잔여 경계 `HandleMscdexRequest`(fwd 502, def 2319–2402)를 외부 링크로 승격(익명 밖 재배치, execution_internal.h 선언). MSCDEX 모듈(증분 8)에서 최종 이동 예정.
- 헤더 선언은 DOS 블록의 최상위 정의 시그니처를 자동 추출해 생성(25개).
- `RecordDosResize`의 기본 인자 2개는 헤더 선언에만 두고 .cpp 정의에서 제거(C2572 방지).

`execution_trampoline.cpp`: 9,855 → 8,686줄.

## 검증

```
cmake --build build/win32_x86_dpmi --config Debug --target repiu_loader_win32  # green
```
첫 빌드가 RecordDosResize 기본 인자 재정의를 잡아냈고 수정 후 링크 성공.

## 다음
증분 8: MSCDEX(패킷·MSF/LBA·HandleMscdex*) + DPMI INT 31h·마우스 INT 33h.
