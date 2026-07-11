# SIB memory 주소 해석 작업 지시

1. 공용 ModR/M decoder에 SIB와 absolute disp32를 추가한다.
2. 모든 기존 memory handler가 확장 decoder를 사용하게 한다.
3. 빌드 후 `+0xF8405` 통과와 새 frontier를 관찰한다.
4. 분석 및 작업 로그를 갱신하고 커밋한다.

# SIB Memory Address Decoding Work Order

Add SIB and absolute disp32 to the shared decoder, retain all existing consumers, build, observe passage beyond `+0xF8405`, document, and commit.
