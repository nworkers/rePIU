# 작업 지시 20260905-590 — Linux x64 return cache-miss 재진입

설계: [20260905-590](../design/20260905-590-linux-x64-return-cache-miss-reentry.md)

## 변경

`LinuxX64EngineResolver`가 공용 AOT transfer resolver를 사용하도록 바꾸고, return trace가
성공 cache target을 기록하게 합니다.

## 검증

1. Linux x64 `repiu` 빌드
2. `REPIU_GUEST_WATCH=0x010F4A96 REPIU_LINUX_X64_RETURN_TRACE=1` watched `pumpit2a`
3. 분석·작업 로그 갱신 및 커밋

---

# Work order 20260905-590 — Linux x64 return cache-miss reentry

Design: [20260905-590](../design/20260905-590-linux-x64-return-cache-miss-reentry.md)

## Change

Make `LinuxX64EngineResolver` use the shared AOT transfer resolver and trace its
successful cache target.

## Verification

1. Build Linux x64 `repiu`.
2. Run watched `pumpit2a` with return tracing.
3. Update analysis and work log, then commit.
