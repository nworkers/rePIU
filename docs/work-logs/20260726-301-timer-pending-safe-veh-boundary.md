# 20260726-301 작업 로그: 타이머 pending 안전 VEH 경계 전달 / Work log

설계: [20260726-301-timer-pending-safe-veh-boundary.md](../design/20260726-301-timer-pending-safe-veh-boundary.md)

작업 지시: [20260726-301-timer-pending-safe-veh-boundary.md](../work-orders/20260726-301-timer-pending-safe-veh-boundary.md)

## 한국어

### 결과

Task 299의 poll-thread TF rendezvous를 제거했습니다. poll loop는 DOS tick을 갱신하고
`timer_interrupt_pending`만 설정합니다. guest thread의 기존 single-step VEH 경계는
HLE/AOT 상태를 guest EIP로 정리한 뒤, native fast path 또는 linear span에 다시 들어가기
전에 공용 `InjectPendingInterrupts`를 호출합니다.

원본 ISR `0x03042EAE`, `IRETD`, IF gate, pending coalescing은 유지했습니다.

### 원인 증거

사용자 장시간 로그와 Windows Application Error를 대조한 결과 세 번의 종료가 모두 다음
형태였습니다.

- 서로 다른 마지막 guest arm EIP
- 대응 wakeup 없는 마지막 `Armed INT 8 VEH wakeup`
- 동일한 `EXCEPTION_SINGLE_STEP(0x80000004)`
- 동일한 Windows fault 주소 `0x6FAC40B1`
- AOT cache 주소 `0x0DB70000`
- 기존 단조로운 `ESP-12` 누수 없음

따라서 특정 guest 명령이 아니라 강제로 설정한 TF가 VEH에서 소비되지 않는 것이 반복 종료의
직접 원인이었습니다.

### 구현

| 파일 | 변경 |
|---|---|
| `live_telemetry_snapshot.cpp` | timer 전달용 suspend/context 조회/TF 설정 제거, pending만 기록 |
| `execution_trampoline.cpp` | wakeup 전용 VEH 분기 제거, 기존 single-step guest 경계에서 pending 전달 |
| `thread_context.h` | `timer_interrupt_wakeup_armed` 제거 |
| `docs/analysis/` | 세 번의 동일 APPCRASH와 새 전달 정책 기록 |

### 검증

Task 299에서 만든 VS2022 Win32 Debug tree와 사용자가 실행하는
`build/win32_x86_debug` VS2026 tree를 모두 빌드했습니다. 두 빌드 모두 성공했으며 기존
C4819 경고 외 compile/link 오류는 없었습니다.

EEPROM fixture를 격리 복사하고 다음 조건으로 150초 실행했습니다.

- backend: `aot-dbt`
- timeout: 150,000 ms
- `REPIU_TIMER_INJECT_LOG=1`
- 별도 `REPIU_EEPROM_PATH`

| 항목 | 결과 |
|---|---:|
| 종료 | 정상 timeout, process exit 0 |
| 기존 125초 frontier | 통과 |
| 강제 TF arm / wakeup | 0 / 0 |
| INT 8 주입 / chain HLE | 2,283 / 2,283 |
| exception caught / malformed | false / 0 |
| diagnostic progress | 129,810 |
| single-step 경계 | 1,094,406 |
| Glide gate | 13,932 / 13,932 |
| 새 Windows APPCRASH | 0 |
| EEPROM hash 변화 | 없음 |

AOT return fallback 747건은 모두 `quarantine` 분류였고 non-guest/unknown은 0이며
`AOT last fallback address`도 0이었습니다. 이번 타이머 변경의 잘못된 return 주소 증거는
없습니다.

### 제한

격리 실행에는 사용자 gameplay 입력을 재현하지 않았습니다. 다만 실패를 만든 강제 TF arm
경로 자체가 코드에서 제거됐고, 150초 동안 자연 VEH 경계만으로 기존보다 많은 INT 8
chain과 progress가 지속됐습니다. 사용자 입력을 포함한 다음 장시간 실행은 실제 gameplay
경로의 최종 확인으로 사용합니다.

---

## English

### Result

Removed Task 299's poll-thread TF rendezvous. The poll loop now updates the DOS
tick and sets only the coalesced pending flag. Existing guest-thread
single-step VEH boundaries deliver pending INT 8 after HLE/AOT reconciliation
and before native fast-path or linear-span re-entry. The original ISR,
`IRETD`, IF gate, and pending semantics remain unchanged.

Three user runs had different final guest arm EIPs but the same unmatched arm,
unhandled `0x80000004`, and Windows fault address `0x6FAC40B1`. The AOT cache
was at `0x0DB70000`, and the old `ESP-12` leak did not recur. This identifies
forced TF escape, rather than a guest instruction, as the direct repeated
failure.

Both the VS2022 Win32 Debug verification tree and the user-facing
`build/win32_x86_debug` VS2026 tree built successfully. An isolated 150-second
`aot-dbt` run passed the former 125-second frontier and ended by normal timeout
with process exit 0: zero TF arms/wakeups, 2,283 INT 8 injections and chain
completions, no caught or malformed exception, progress 129,810, 1,094,406
single-step boundaries, 13,932/13,932 Glide gates, and no new Windows APPCRASH.
The EEPROM hash was unchanged.

All 747 AOT return fallbacks were classified as quarantine; non-guest and
unknown were zero, and the last fallback address was zero. The isolated run
did not reproduce user gameplay input, so the next interactive long run is the
final gameplay-path confirmation.
