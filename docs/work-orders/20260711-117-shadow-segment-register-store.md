# shadow segment register store 작업 지시

1. 관찰된 `8C /r`, mod=3 형식을 decode한다.
2. shadow segment selector를 목적 register 하위 16비트에 기록한다.
3. segment store 진단을 갱신한다.
4. 빌드, hello, PIU 반복 실행으로 ES=`0x2C` 전달과 다음 frontier를 확인한다.
5. 문서와 작업 로그를 갱신하고 커밋한다.

# Shadow Segment Register Store Work Order

Decode observed register-direct opcode-8C forms, write the shadow segment selector into the destination register's low 16 bits, update segment-store diagnostics, validate the build/hello/PIU paths and ES=`0x2C` transfer, document the next frontier, and commit.
