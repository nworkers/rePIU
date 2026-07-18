# 20260718-233 작업 로그 — Phase 1 증분 6 (공유 substrate 승격, prep)

관련: [작업 지시서](../work-orders/20260718-233-execution-trampoline-decomposition-order.md) · [증분5 로그](20260718-233-execution-trampoline-phase1-increment5-log.md)

## 한 일 / What was done

DOS 클러스터 분석에서, 이후 서비스 클러스터(DOS·DPMI·MSCDEX·세그먼트)가 **공용 헬퍼 substrate**를 공유함을 확인했다. 이를 먼저 외부 링크로 승격해 후속 추출을 단순화했다(모듈 신설·CMake 변경 없음, 순수 링크 승격).

승격 대상(익명 네임스페이스 밖으로 재배치 + `execution_internal.h` 선언, forward 선언 제거):
- `RecoverFromHleExit`
- `RecordHandledDosInterrupt` (forward 1012, 정의 2826)
- `RecordLowMemoryAccess`
- `ReadGuestSegmentSelector` (forward 1803, 정의 3070)

## 근거 / Rationale

증분 1~5는 경계 심볼이 1~2개였으나, DOS부터는 여러 클러스터가 공유하는 substrate가 있어, 서비스 클러스터를 개별 추출할 때마다 substrate를 조금씩 승격하면 중복·위험이 커진다. foundational-first 원칙에 따라 substrate를 먼저 승격한다. 이 4개는 여러 영역에서 쓰이므로 특정 모듈로 옮기지 않고 트램폴린에 외부 링크로 남긴다(추후 소유 모듈이 정해지면 이동 검토).

## 검증 / Verification

```
cmake --build build/win32_x86_dpmi --config Debug --target repiu_loader_win32  # green
```
파일 크기 변화 없음(9,853→9,855, 선언 추가분). 링크 성공.

## 다음 / Next

증분 7: DOS INT 21h/2Fh 서비스 추출(이제 substrate가 외부 링크라 깨끗하게 분리 가능).
