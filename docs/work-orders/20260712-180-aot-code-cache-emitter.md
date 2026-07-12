# AOT 코드 캐시 emitter 작업 지시

1. AOT 계획에 block 및 instruction 레코드를 보존합니다.
2. 플랫폼 공용 code-cache image, 주소 맵, fixup 자료구조를 추가합니다.
3. direct control flow를 `rel32`로 emit하고 내부 target을 해결합니다.
4. HLE/간접 경계를 안전한 sentinel과 외부 fixup으로 표현합니다.
5. probe 출력에 생성 및 검증 결과를 추가합니다.
6. Win32 x86 Debug 빌드와 PIU/OpenWatcom 표본을 검증합니다.
7. 분석·아키텍처·작업 로그를 갱신하고 커밋합니다.

# AOT Code Cache Emitter Work Order

Persist instruction-level plan records, emit a platform-neutral code-cache image with bidirectional address mapping and fixups, resolve direct rel32 edges, represent HLE and indirect boundaries safely, extend the probe, verify representative binaries and Win32 x86 Debug, update documentation, and commit.
