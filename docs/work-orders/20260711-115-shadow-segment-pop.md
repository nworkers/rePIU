# shadow segment POP 동기화 작업 지시

1. PIU의 임시 DS 전환과 `POP DS` 복원 위치를 정적으로 검증한다.
2. single-step 경로에 관찰된 32비트 opcode `1F` handler를 추가한다.
3. selector, ESP, EIP와 segment trace를 일관되게 갱신한다.
4. Win32 x86 빌드와 hello sample을 검증한다.
5. PIU 반복 실행에서 DS 복원 trace와 다음 frontier를 확인한다.
6. 분석 문서와 작업 로그를 갱신하고 커밋한다.

# Shadow Segment POP Synchronization Work Order

Verify PIU's temporary DS switch and POP DS restoration, add an observed-form 32-bit opcode-1F handler to the single-step path, update selector/ESP/EIP and segment trace consistently, validate the Win32 x86 build and hello sample, repeat PIU runs to confirm DS restoration and the next frontier, update documentation, and commit.
