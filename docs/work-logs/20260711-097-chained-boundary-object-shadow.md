# Arena 경계 객체 shadow chain 작업 로그

## 결과

최초 arena 경계 객체에서 시작한 연속 객체 배열을 base/frontier 관계로 추적하도록 구현했습니다. 다음 객체 base가 직전 frontier와 정확히 같고, 같은 객체의 store가 base 이후 64바이트 안에 있을 때만 기존 `66 C7`, `C7`, `89`, `D9` handler가 shadow memory를 사용합니다.

최초 경계 시점의 `ESI=0x640`, `EDX=0x2C`에서 배열 span `0x11300`을 계산합니다. 64바이트 이상 1 MiB 이하인 span만 chain 상한으로 채택합니다. memory-store HLE도 diagnostic progress counter를 증가시키며, 기존 quiet polling 100,000회와 독립적인 1초 실행 timeout은 유지했습니다.

## 관찰

1초 진단 창 안에서 객체 배열 처리가 5자리 store 수와 4자리 이상의 shadow read hit까지 진행됩니다. 실행 시점에 따라 생성자 `0x0001E1xx`에서 관찰이 끝나거나 배열 초기화를 마치고 `stage.cfg`를 연 뒤 allocator `0x000F7Axx`까지 도달합니다.

대표적인 후속 blocker는 relocated base + `0x000F7A71`의 `8B 16`입니다. 이때 `ESI=0`이므로 source는 low-memory/null address `0`이며, 객체 chain과 별도의 low-memory load 요구사항입니다.

회귀 테스트는 시점 의존적인 마지막 명령 하나를 고정하지 않고 다음 안정적인 진척을 검증합니다.

* handled/shadow store count: 5자리
* shadow memory read hit count: 4자리 이상
* shadow memory byte count: 5자리
* DOS path trace: `intro.ani` open까지 필수, `stage.cfg` open은 도달 시 허용
* 관찰 지점: 생성자 `0x0001E1xx` 또는 후속 allocator `0x000F7Axx`

## 검증

* Win32 x86 Debug 빌드: 성공
* `dos4gw_hello`: `Hello, world!` 출력 후 정상 반환
* `piu_1st`: 5자리 연쇄 store와 4자리 이상의 shadow read hit 관찰
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`: 전체 성공

# Arena-Boundary Object Shadow Chain Work Log

## Result

Implemented base/frontier tracking for the contiguous object array starting at the first arena-boundary object. Existing `66 C7`, `C7`, `89`, and `D9` handlers use shadow memory only when the next object base exactly matches the prior frontier and stores for the same object remain within 64 bytes after its base.

The initial boundary state `ESI=0x640`, `EDX=0x2C` produces array span `0x11300`. Only spans validated between 64 bytes and 1 MiB become the chain limit. Memory-store HLE now increments the diagnostic progress counter while retaining the existing 100,000-iteration quiet limit and independent one-second execution timeout.

## Observation

Within the one-second diagnostic window, object-array processing reaches a five-digit store count and at least a four-digit shadow-read-hit count. Depending on observation timing, execution either stops in the constructor at `0x0001E1xx` or completes array initialization, opens `stage.cfg`, and reaches the allocator at `0x000F7Axx`.

A representative following blocker is `8B 16` at relocated base + `0x000F7A71`. With `ESI=0`, its source is low-memory/null address `0`, which is a separate low-memory load requirement rather than part of the object chain.

The regression test verifies stable progress instead of fixing one timing-dependent final instruction:

* handled/shadow store count: five digits
* shadow memory read hit count: at least four digits
* shadow memory byte count: five digits
* DOS path trace: `intro.ani` open is required; `stage.cfg` open is accepted when reached
* observation point: constructor `0x0001E1xx` or following allocator `0x000F7Axx`

## Verification

* Win32 x86 Debug build: passed
* `dos4gw_hello`: returned normally after printing `Hello, world!`
* `piu_1st`: observed five-digit chained stores and at least four-digit shadow read hits
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`: all tests passed
