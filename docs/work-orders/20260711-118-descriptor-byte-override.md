# descriptor-backed segment override byte 작업 지시

1. 8비트 register read/write helper와 CMP flag 갱신을 구현한다.
2. `ReadSegmentByte`에 selector translation fallback을 추가한다.
3. 관찰된 `26 3A 10`, `26 8A 30` 형식을 처리한다.
4. 빌드, hello, PIU 반복 실행으로 다음 frontier를 확인한다.
5. 분석 문서와 작업 로그를 갱신하고 커밋한다.

# Descriptor-Backed Segment-Override Byte Work Order

Implement 8-bit register helpers and CMP flags, add selector translation fallback to `ReadSegmentByte`, handle observed `26 3A 10` and `26 8A 30` forms, validate build/hello/repeated PIU execution, document the next frontier, and commit.
