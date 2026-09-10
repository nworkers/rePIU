# 20260911-655 작업 지시: legacy fallback moffs32 store HLE

설계: [20260911-655 설계](../design/20260911-655-legacy-moffs32-store-hle.md)

## 한국어

1. 공용 memory-store HLE에 prefix 없는 `A3 disp32`/EAX store를 추가합니다.
2. 기존 guest writable/write/provenance 경계를 재사용합니다.
3. 정상 write·EIP·EFLAGS와 arena 밖 거부를 합성 probe로 검증합니다.
4. Linux x64 Debug 빌드, 전체 core probe, 실제 `pumpit2a`를 실행합니다.
5. 아키텍처·분석·작업 로그를 갱신하고 커밋합니다.

## English

1. Add unprefixed `A3 disp32`/EAX stores to the shared memory-store HLE.
2. Reuse the existing guest-writable, write, and provenance boundaries.
3. Probe the successful write, EIP, EFLAGS, and outside-arena refusal.
4. Build Linux x64 Debug, run all core probes, and run real `pumpit2a`.
5. Update architecture, analysis, and the work log, then commit.
