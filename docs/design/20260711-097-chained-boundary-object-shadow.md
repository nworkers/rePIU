# Arena 경계 객체 shadow chain 설계

## 배경

첫 경계 객체는 base `arena end-4`에서 시작해 마지막 필드 `base+0x28`까지 초기화되었다. 다음 blocker의 base는 `arena end+0x28`이며, 이는 이전 base에 객체 stride `0x2C`를 더한 값이자 이전 객체가 기록한 shadow tail의 바로 다음 주소다.

명령 흐름은 기존 분석에서도 반복된 `66 C7`, `C7`, `D9`, `89` 객체 초기화 패턴과 일치한다. 따라서 새 base는 임의 allocator 주소가 아니라 경계에서 시작한 연속 객체 배열의 다음 원소로 판단한다.

## 설계

ThreadContext에 현재 boundary object base와 연속 shadow frontier를 기록한다.

* 최초 base는 반드시 실제 arena 내부 마지막 64바이트에 있어야 한다.
* 다음 객체 base는 직전 shadow frontier와 정확히 같을 때만 chain으로 받아들인다.
* 같은 객체의 destination은 현재 base부터 `base+64` 이내여야 한다.
* 전체 chain은 arena end 이후 4 KiB 이내로 제한한다.
* 지원 opcode는 기존 decoder가 있는 `66 C7 /0`, `C7 /0`, `89 /r`, `D9 /2-/3`으로 제한한다.
* 각 store 뒤 frontier를 실제 기록 끝 주소까지 전진시킨다.

이 정책은 연속 객체 생성 패턴만 보존하며, 간격이 있거나 4 KiB를 넘는 임의 arena 외부 주소는 계속 거부한다.

# Arena-Boundary Object Shadow Chain Design

## Background

The first boundary object starts at `arena end-4` and initializes through its final field at `base+0x28`. The next blocker base is `arena end+0x28`, which equals the previous base plus object stride `0x2C` and is immediately after the previous object's recorded shadow tail.

The instruction flow matches the repeated `66 C7`, `C7`, `D9`, and `89` object initialization pattern observed in earlier analysis. The new base is therefore treated as the next element of a contiguous object array that began at the boundary, not as an arbitrary allocator address.

## Design

Record the current boundary-object base and contiguous shadow frontier in `ThreadContext`.

* The initial base must remain within the final 64 bytes of the real arena.
* Accept a next object base only when it exactly equals the previous shadow frontier.
* Destinations for the same object must remain between the current base and `base+64`.
* Record `ESI` count multiplied by `EDX` stride at the first boundary as the array span. The current values produce `0x640 * 0x2C = 0x11300`.
* Use the calculated span only when it is between 64 bytes and 1 MiB; otherwise use a 4 KiB fallback.
* Limit opcodes to existing decoders for `66 C7 /0`, `C7 /0`, `89 /r`, and `D9 /2-/3`.
* Advance the frontier to the actual end of each recorded store.

This preserves only contiguous object construction. Gapped or arbitrary out-of-arena addresses and chains beyond the validated array span remain rejected.

Memory-store handling previously did not increment the diagnostic progress counter, so the quiet limit could expire while the chain was actively advancing. Count each handled memory store as diagnostic progress and retain the original 100,000-iteration quiet limit and independent one-second execution timeout.
