# AOT 기본 블록 fall-through 연결 작업 로그

`kCopy`로 끝나는 emitted basic block 뒤에 `E9 rel32` link를 추가했습니다. direct target이 image에 없으면 cache image build를 실패시킵니다. PIU는 이전의 즉시 access violation 대신 제한 시간까지 실행했고 return trace도 정상화되었습니다.

# AOT Basic-Block Fall-through Linking Work Log

Added an `E9 rel32` link after each emitted basic block ending in `kCopy`. A direct target missing from the image now fails cache-image construction. PIU ran until the bounded timeout instead of the previous immediate access violation, and its return trace normalized.
