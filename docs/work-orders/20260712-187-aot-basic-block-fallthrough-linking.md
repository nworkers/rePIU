# AOT 기본 블록 fall-through 연결 작업 지시

1. cache fixup에 block-fallthrough 종류를 추가합니다.
2. `kCopy`로 끝나는 모든 basic block 뒤에 `E9 rel32`를 발행합니다.
3. target을 image address map으로 resolve하고 외부 target은 image 오류로 처리합니다.
4. probe로 `F8770` 뒤가 `F8772` cache entry로 연결되는지, PIU가 기존 즉시 예외를 넘는지 검증합니다.

# AOT Basic-Block Fall-through Linking Work Order

1. Add a block-fallthrough fixup kind.
2. Emit `E9 rel32` after every basic block ending in `kCopy`.
3. Resolve its target through the image address map and treat an external target as an image error.
4. Verify that `F8770` links to the `F8772` cache entry and that PIU advances beyond the previous immediate exception.
