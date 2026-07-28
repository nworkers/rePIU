# 20260729-348 AOT busy-wait 타이머 safe point 작업 로그 / Work log

## 한국어

### 결과

`pumpit1`의 AOT 무경계 tick 대기를 일반적인 back-edge safe point로 해소했습니다.
원본 실행 파일, tick 변수, INT 8 ISR, `IRETD`는 변경하지 않았습니다. poll thread는
coalesced pending과 placement request만 게시하고, 실제 interrupt frame 작성과 ISR 전환은
guest thread의 기존 `InjectPendingInterrupts`가 담당합니다.

### 구현

- AOT build option, image/site metadata, placement trap index를 추가했습니다.
- direct/conditional back edge 앞에 GPR을 사용하지 않는 15바이트
  `pushfd`/request guard/`popfd`/`INT3`를 생성했습니다.
- initial placement와 dynamic append에서 placement 소유 request word 주소를 해결했습니다.
- 55ms DOS tick 게시 시 `InterlockedExchange`로 safe-point request를 arm합니다.
- 전용 breakpoint handler를 일반 AOT reentry보다 먼저 호출하고, request를 지운 뒤
  `ExceptionAddress + 1`을 resume EIP로 사용해 공용 INT 8 주입기를 호출합니다.
- 자연 VEH 경계에서 pending을 먼저 소비한 경우 request도 지워 stale trap을 방지했습니다.
- site 수와 trap/injected/deferred 카운터를 loader 시작/종료 로그에 추가했습니다.
- `repiu_aot_probe --timer-safe-point`가 conditional/direct back edge, on/off 생성,
  guard 바이트와 placement 주소 해결을 검증합니다.

### 검증과 관찰

| 검증 | 결과 |
|---|---|
| Win32 x86 Debug clean-first 및 최종 증분 빌드 | 성공 |
| `repiu_aot_probe --timer-safe-point` | `timer_safe_point_probe=true` |
| 5초 `aot-dbt` smoke | heartbeat `43,192 → 83,870`, trap/injected/deferred `50/0/50` |
| 입력 포함 50초 `pumpit1` | trap/injected/deferred `518/452/66`, original fatal `0` |
| 기존 정지 구간 이후 | heartbeat `261,280@37s → 278,446@50s` |
| 기존 정지 구간 이후 | dispatch `130,640@37s → 139,223@50s` |

첫 live 검증에서는 Win32 breakpoint 처리 후 EIP가 같은 `INT3`에 남아 1.2초 동안
trap `134,721`회가 반복됐습니다. handler가 소유 sentinel의 resume EIP를
`ExceptionAddress + 1`로 명시하도록 수정했고, 5초 smoke에서 50회로 정상화했습니다.

빌드 중에는 placement 헤더보다 오래된 `aot_page_coherence_win32.obj`가 남아 구조체 ABI가
불일치하는 증분 빌드 문제가 한 번 발생했습니다. clean-first 재빌드 후 프로브와 실게임
모두 정상 동작했으며, 소스 결함과 구분했습니다.

### 남은 범위

이번 검증은 관찰된 direct/conditional back-edge 대기를 다룹니다. 간접 분기나 return만으로
구성된 별도 무경계 루프가 확인되면 같은 request/site/handler 구조로 확장해야 합니다.

---

## English

### Result

Resolved the boundary-free AOT tick wait in `pumpit1` with general back-edge safe points.
The original executable, tick variable, INT 8 ISR, and `IRETD` remain unchanged. The poll
thread publishes only the coalesced pending bit and placement request; the existing guest-thread
`InjectPendingInterrupts` remains the only path that creates the frame and enters the ISR.

### Implementation

- Added an AOT build option, image/site metadata, and a placement trap index.
- Emitted a GPR-free 15-byte `pushfd`/request-guard/`popfd`/`INT3` sequence before direct and
  conditional back edges.
- Resolved one placement-owned request address in both initial placement and dynamic appends.
- Armed the request with `InterlockedExchange` when publishing each 55ms DOS tick.
- Ran the dedicated breakpoint handler before generic AOT reentry, cleared the request,
  resumed from `ExceptionAddress + 1`, and delegated to the common INT 8 injector.
- Cleared the request when a natural VEH boundary consumed pending first, preventing stale traps.
- Added startup site counts and final trap/injected/deferred diagnostics.
- Added `repiu_aot_probe --timer-safe-point` coverage for conditional/direct back edges,
  enabled/disabled emission, guard bytes, and placement address resolution.

### Verification

The Win32 x86 Debug clean-first and final incremental builds succeeded, and the standalone probe
reported `timer_safe_point_probe=true`. A five-second smoke run advanced heartbeat from 43,192 to
83,870 and recorded `50/0/50` traps/injections/deferrals. The 50-second interactive run recorded
`518/452/66` and zero original fatal events. Across the previous freeze window, heartbeat advanced
from 261,280 at 37 seconds to 278,446 at 50 seconds, while dispatch advanced from 130,640 to
139,223.

An initial live run left EIP on the same Win32 breakpoint and retrapped 134,721 times in 1.2
seconds. Explicitly resuming the owned sentinel at `ExceptionAddress + 1` reduced the five-second
smoke run to 50 traps. A stale `aot_page_coherence_win32.obj` also caused one incremental-build ABI
mismatch after the placement header changed; a clean-first rebuild separated that build artifact
issue from the source behavior.

### Remaining scope

This verification covers the observed direct and conditional back-edge wait. If a distinct
boundary-free loop formed only by indirect transfers or returns is confirmed, the same
request/site/handler structure can be extended in a separate task.
