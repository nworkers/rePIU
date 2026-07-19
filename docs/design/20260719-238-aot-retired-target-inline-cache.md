# AOT retire target inline-cache coherence design

## 목적 / Purpose

동적 AOT의 guest page retire 뒤에도 다른 번역 블록의 indirect/return inline cache가
retired page의 이전 native cache 주소로 직접 점프하지 않도록 합니다.

Prevent indirect and return inline caches in other translated blocks from jumping
directly to an old native cache address of a retired guest page.

## 확인된 결함 / Confirmed gap

`RetireWin32AotGuestPage`는 retire 대상 페이지에 속한 address-map entry의 첫 바이트만
`INT3`로 바꿉니다. 반면 inline-cache slot은 guest target immediate와 native `E9 rel32`
target을 별도로 보관하므로, 이미 학습된 slot은 해당 entry의 첫 바이트를 다시 거치지
않습니다. 따라서 retired target으로의 cache hit가 unmapped 또는 inactive native code로
진입할 수 있습니다.

`RetireWin32AotGuestPage` currently changes only the first byte of address-map entries
that belong to the retired page to `INT3`. An inline-cache slot separately retains its
guest-target immediate and native `E9 rel32` target, so a learned slot bypasses that
entry byte. A hit can therefore enter inactive or unmapped native code.

## 정책 / Policy

retire 중 cache가 writable인 동일한 보호 전환 구간에서, guest target이 retire page에
속한 모든 inline-cache entry의 guard를 초기 miss 형식인 `E9 rel32 miss-tail`로
복원합니다. target immediate와 jump displacement는 남겨 두되 guard가 miss를 강제하므로
다음 전송은 dispatcher에서 현재 generation을 다시 resolve하고 정상 patch protocol으로
갱신됩니다.

While the cache is writable during retirement, reset the guard of every inline-cache
entry whose guest target is in the retired page to the initial `E9 rel32 miss-tail`
form. Keep the target immediate and displacement; the guard forces the next transfer
through the dispatcher, which resolves the current generation and repatches normally.

이 변경은 원본 guest 코드나 Glide ABI를 변경하지 않으며, Win32 AOT cache coherence
계층에만 한정됩니다.

This changes neither original guest code nor the Glide ABI and is confined to the Win32
AOT cache-coherence layer.

## 검증 / Verification

1. `aot_probe` coherence 검증에 retired target을 가진 inline-cache slot이 miss로
   복원되는지 확인하는 단언을 추가합니다.
2. Win32 x86 Debug 전체 빌드를 수행합니다.
3. `pumpit1`을 `aot-dynamic` backend로 최대 180초 실행합니다.
4. 기존 `0x0304ED35` return cache-hit AV가 사라지는지와 다음 실행 frontier를 기록합니다.

1. Extend `aot_probe` coherence coverage to assert that a slot targeting a retired page
   is restored to a miss.
2. Build the full Win32 x86 Debug target.
3. Run `pumpit1` with the `aot-dynamic` backend for up to 180 seconds.
4. Record whether the prior `0x0304ED35` return-cache-hit AV disappears and capture the
   next execution frontier.

## 구현 상태 / Implementation Status

- 2026-07-19 (Task 245): 본 설계는 Task 238 시점에는 문서로만 존재했고 코드에 구현되지
  않았음이 확인되었다. Task 245에서 `RetireWin32AotGuestPage`에 guard 리셋을 구현한다.
  구현은 retire 페이지에 target immediate가 속한 설치된 guard(`0F 85`)를 초기
  `E9 rel32 miss-tail` + `90` 형태로 복원하고, 리셋 수를
  `Win32AotGuestPageRetireResult::guard_reset_count`로 보고한다.

- 2026-07-19 (Task 245): This design existed only as a document at Task 238 time and was
  confirmed unimplemented in code. Task 245 implements the guard reset in
  `RetireWin32AotGuestPage`: every installed guard (`0F 85`) whose target immediate lies
  in the retired page is restored to the initial `E9 rel32 miss-tail` + `90` form, and
  the reset count is reported via `Win32AotGuestPageRetireResult::guard_reset_count`.
