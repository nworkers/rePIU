# 작업 지시 20260903-581 — guest 주소 watch

설계: [20260903-581](../design/20260903-581-guest-address-watch.md)

## 변경 대상

| 파일 | 변경 |
|---|---|
| `src/engine/telemetry/guest_address_watch.h` | 신규 — 이벤트 종류와 API |
| `src/engine/telemetry/guest_address_watch.cpp` | 신규 — 게이트, counter, 출력 |
| `CMakeLists.txt` | 새 source 등록 |
| `src/engine/execution/execution_trampoline.cpp` | `step`·`priv`·`fault` 계측 |
| `src/engine/aot/aot_runtime_dispatch.cpp` | `dispatch`·`cache` 계측 |
| `docs/analysis/linux-port-frontier.md` | 3.28절 |

## 구현 단계

1. `guest_address_watch.h`에 `GuestAddressWatchEvent`(다섯 값)와 두 함수를
   선언합니다 — 게이트 조회, 이벤트 기록.
2. `guest_address_watch.cpp`에서 `REPIU_GUEST_WATCH`를 한 번만 읽어 정적으로
   보관합니다. 값이 없거나 0이면 감시는 꺼집니다.
3. 기록 함수는 게이트를 **가장 먼저** 확인하고, 꺼져 있으면 즉시 돌아옵니다.
4. 이벤트 종류마다 counter를 올리고, 처음 몇 번만 `[repiu-watch]` 줄을 찍습니다.
5. `RecordSingleStepState`의 guest-EIP 분기 안에서 `step`을 기록합니다.
6. `HandlePrivilegedTrapInstruction`이 `0xFA`/`0xFB`를 서비스한 자리에서 `priv`를
   기록합니다.
7. `DispatchGuestFault` 진입에서, `Eip`가 cache 안이고 그 cache 주소의 guest
   주소가 감시 대상이면 `fault`를 기록합니다.
8. `ResolveAotTransferTarget`에서 `target`이 감시 대상이면 `dispatch`를, 성공
   반환 직전에 `cache`를 기록합니다.
9. `REPIU_GUEST_WATCH`가 없으면 어떤 실행도 **한 줄도** 달라지지 않아야 합니다.

## 검증 절차

1. Linux i386 `repiu`를 감시 없이 `pumpit2a`로 돌리고, `[repiu-watch]` 줄이
   없음과 Task 580과 같은 진행을 확인합니다.
2. 같은 실행을 `REPIU_GUEST_WATCH=0x010F1728`로 반복하고, 어떤 이벤트가 어떤
   순서로 나오는지 그대로 기록합니다.
3. Linux i386 `repiu_core_probe`.
4. Linux x64 `repiu_core_probe`.
5. Win32 회귀는 빌드가 가능한 범위에서만 수행하고, 불가능하면 이유를 로그에
   남깁니다.

## 완료 조건

- **i386이 guest `0x010F1728`을 어떤 경로로 실행하는지 관측으로 기록됩니다.**
- Task 580이 남긴 추정이 확인되거나 반증됩니다.
- Task 580의 두 방향 중 어느 쪽이 근거를 얻는지 작업 로그가 적습니다. 구현은
  하지 않습니다.
- 작업 로그와 frontier 3.28절.

---

# Work order 20260903-581 — A guest-address watch

Design: [20260903-581](../design/20260903-581-guest-address-watch.md)

## Files to change

| File | Change |
|---|---|
| `src/engine/telemetry/guest_address_watch.h` | New — event kinds and API |
| `src/engine/telemetry/guest_address_watch.cpp` | New — gate, counters, output |
| `CMakeLists.txt` | Register the new source |
| `src/engine/execution/execution_trampoline.cpp` | The `step`, `priv` and `fault` hooks |
| `src/engine/aot/aot_runtime_dispatch.cpp` | The `dispatch` and `cache` hooks |
| `docs/analysis/linux-port-frontier.md` | Section 3.28 |

## Implementation steps

1. Declare `GuestAddressWatchEvent` (five values) and two functions in
   `guest_address_watch.h` — the gate query and the event record.
2. In `guest_address_watch.cpp`, read `REPIU_GUEST_WATCH` once and hold it
   statically. Absent or zero means the watch is off.
3. The record function checks the gate **first** and returns immediately when
   it is off.
4. Per event kind, bump a counter and print a `[repiu-watch]` line for the
   first few occurrences only.
5. Record `step` inside the guest-EIP branch of `RecordSingleStepState`.
6. Record `priv` where `HandlePrivilegedTrapInstruction` services `0xFA`/`0xFB`.
7. At `DispatchGuestFault` entry, record `fault` when `Eip` is inside the cache
   and that cache address maps to the watched guest address.
8. In `ResolveAotTransferTarget`, record `dispatch` when `target` is the watched
   address, and `cache` just before a successful return.
9. Without `REPIU_GUEST_WATCH`, no run may differ by **a single line**.

## Verification

1. Run the Linux i386 `repiu` on `pumpit2a` with the watch off; confirm no
   `[repiu-watch]` line and the same progress Task 580 recorded.
2. Repeat the same run with `REPIU_GUEST_WATCH=0x010F1728` and record exactly
   which events appear, in what order.
3. Linux i386 `repiu_core_probe`.
4. Linux x64 `repiu_core_probe`.
5. Run the Win32 regression only as far as it can be built here; record the
   reason in the work log if it cannot.

## Completion criteria

- **How i386 executes guest `0x010F1728` is recorded as an observation.**
- Task 580's inference is either confirmed or refuted.
- The work log states which of Task 580's two directions gains support. No
  implementation of either.
- The work log and frontier section 3.28.
