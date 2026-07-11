# DOS/16M resident table 정적 복원 작업 지시

1. MZ header, load image 범위와 relocation 78개를 원본 file offset 및 load-image offset으로 복원한다.
2. BW `next_header_pos` chain을 순회하며 header와 GDT entry를 엄격히 파싱한다.
3. 각 load selector의 source file range, copy size, memory size, access byte를 복원한다.
4. 각 module의 RSI-2 block과 모든 relocation offset을 복원한다.
5. 전체 범위·selector·terminator·padding 불변 조건을 검사하는 정적 분석 도구를 추가한다.
6. JSON manifest와 Mermaid 기반 분석 문서를 생성하고 index/frontier를 갱신한다.
7. manifest 재생성 일치, 빌드, 도구의 malformed-input 실패를 검증하고 작업 로그를 남긴다.

# DOS/16M Resident Table Static Reconstruction Work Order

Recover all 78 MZ relocations, traverse the complete BW chain, derive every selector copy range, decode every RSI-2 relocation entry, enforce structural invariants, generate deterministic JSON and Mermaid-backed analysis documentation, update the analysis indexes, verify reproducibility and malformed-input rejection, build, and record the work.
