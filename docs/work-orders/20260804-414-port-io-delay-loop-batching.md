# Task 414 작업 지시 — 포트 I/O 지연 루프 batching

설계: [20260804-414](../design/20260804-414-port-io-delay-loop-batching.md)

## 변경 파일

| # | 파일 | 변경 |
|---|---|---|
| 1 | `src/platform/win32/io/port_io_delay_loop.h` (신규) | 일치 결과 구조체, 통계 구조체, `TryBatchPortIoDelayLoop`, 통계 접근자, 스위치 해석 |
| 2 | `src/platform/win32/io/port_io_delay_loop.cpp` (신규) | 패턴 디코더와 카운터 전진. 파일 지역 통계 |
| 3 | `src/platform/win32/io/port_io_emulator.cpp` | JAMMA 입력 경로에서 `IN` emulate 후 batching 시도 |
| 4 | `src/host/win32/main.cpp` | 통계 로그 한 줄 |
| 5 | `CMakeLists.txt` | 새 소스 추가 |

**`thread_context.h`와 `execution_trampoline.h`는 건드리지 않습니다.** 두 헤더는 거의
모든 번역 단위가 포함하므로 전체 재빌드(40분 이상)를 부릅니다.

## 구현 규칙

* 일치하지 않으면 **아무 상태도 바꾸지 않습니다.** 실패는 항상 예전 동작으로 수렴합니다.
* EIP·EFLAGS·EAX·EDX를 쓰지 않습니다. **카운터 레지스터 하나만** 전진시킵니다.
* 게스트 메모리 디코드 전에 반드시 `IsGuestRangeReadable`로 확인합니다.
* 화이트리스트 밖 opcode를 만나면 즉시 불일치로 처리합니다(추측 실행 금지).
* 통계는 guest thread 전용이므로 lock 없이 둡니다.

## 검증

1. `cl /Zs` 문법 검사 → 증분 Release 빌드(전체 재빌드가 아니어야 함).
2. **A/B, 같은 세션, EEPROM 실행별 격리** — pumpit3 `REPIU_PORT_IO_DELAY_LOOP=0`(대조)
   3회, 기본값(처리) 3회, 각 60초.
3. 기계 확인: 처리 실행에서 `0x0301DB22`의 port I/O가 약 200배 줄고 batch 통계의
   건너뛴 반복 수가 그에 상응해야 합니다.
4. **회귀:** pumpit1 60초를 같은 세션에서 on/off 각 1회. 오늘 기준선은 410 프레임입니다.
5. 설계 §6의 사전 등록 분기를 따릅니다.

## 산출물

작업 로그, [interrupts and port I/O](../analysis/interrupts-and-port-io.md) 또는
[pumpit3 bring-up](../analysis/pumpit3-bring-up.md) 갱신,
[pumpit3 기동 중 멈춤](../analysis/pumpit3-startup-stall.md) 갱신, frontier 갱신,
`ARCHITECTURE.md`에 새 경로 반영.

---

# Task 414 Work Order — batching the port I/O delay loop

Design: [20260804-414](../design/20260804-414-port-io-delay-loop-batching.md)

## Files

New `src/platform/win32/io/port_io_delay_loop.h` and `.cpp` hold the match result, the
statistics, `TryBatchPortIoDelayLoop`, the accessor, and the switch;
`port_io_emulator.cpp` calls it after the `IN` is emulated on the JAMMA input path;
`main.cpp` prints one statistics line; `CMakeLists.txt` gains the source. **Neither
`thread_context.h` nor `execution_trampoline.h` is touched**, since both are included nearly
everywhere and would force a forty-minute full rebuild.

## Implementation rules

A mismatch changes **no state at all**, so failure always resolves to the old behaviour.
EIP, EFLAGS, EAX, and EDX are never written — **only the counter register** advances. Guest
bytes are checked with `IsGuestRangeReadable` before decoding, and any opcode outside the
whitelist ends the match immediately rather than being guessed at. The statistics are
guest-thread only and need no locking.

## Verification

Syntax-check, then an **incremental** Release build. Then A/B in one session with the EEPROM
isolated per run: three pumpit3 runs at `REPIU_PORT_IO_DELAY_LOOP=0` against three at the
default, 60 seconds each. Confirm the mechanism first — port I/O at `0x0301DB22` should fall
about 200-fold with a matching count of skipped iterations — then check pumpit1 once per
condition for regression against today's 410-frame baseline, and follow the design's
pre-registered branch.

## Deliverables

The work log, updates to the port I/O and pumpit3 analyses, the frontier, and
`ARCHITECTURE.md`.
