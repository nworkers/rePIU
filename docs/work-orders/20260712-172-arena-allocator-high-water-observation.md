# Arena allocator high-water 관찰 작업 지시 / Work Order

1. 기존 allocator probe/commit 지점을 shared telemetry에 연결합니다.
2. request size, selected block, high-water, arena remaining을 allocation-free 방식으로 기록합니다.
3. `+0x873F4`의 source page state/protection을 기록합니다.
4. Win32/x86 빌드와 짧은 회귀 실행 후 약 341초까지 supervisor로 검증합니다.
5. 결과에 따라 exhaustion, free/lifetime, page protection 중 다음 작업을 선택합니다.

## English

Publish confirmed allocator request/commit telemetry, high-water and remaining bytes, plus source-page state at `+0x873F4`. Verify the Win32/x86 build, a short regression run, and then the repeatable 341-second boundary before selecting the next correction.
