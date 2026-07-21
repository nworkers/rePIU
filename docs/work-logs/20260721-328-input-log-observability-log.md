# 20260721-328-input-log-observability-log

## 작업 결과 요약 (Summary)

INT 8 타이머 주입 로그를 env 게이팅으로 기본 비활성화하고, 키 입력이 실제로
게스트에 전달되는지 확인할 수 있는 엣지 트리거 입력 로그를 추가했습니다.
실기 구동으로 press/release가 게스트까지 도달하는 것을 end-to-end로 확인했으며,
이 결과는 Task 327(EIP 전진 수정)이 실제로 동작함을 함께 증명합니다.

Gated the INT 8 timer-injection log behind an env var and added edge-triggered
input logging. A live run confirmed press/release reach the guest end-to-end,
which simultaneously proves the Task 327 EIP-advance fix works.

## 변경 파일 (Changes)

* `src/platform/win32/execution/execution_trampoline.cpp`
  - `Injected INT 8` 로그를 `REPIU_TIMER_INJECT_LOG` 게이팅(기본 off).
* `src/platform/win32/io/port_io_emulator.cpp`
  - `JammaBitName` 비트 이름표 3종(IN0/SYSTEM/IN1), 포트 상수화.
  - `LogJammaInputTransition`: 포트별 최초 폴링 1회 + 엣지 트리거 press/release.

## 검증 (Verification) — 실기 구동 완료

빌드: `cmake --build build/win32_x86_debug --target repiu_loader_win32 / repiu_supervisor_win32 --config Debug` 성공.
구동: `repiu_supervisor_win32.exe pumpit1`, `REPIU_EXECUTION_BACKEND=aot-dynamic`.

### 1. INT 8 로그 억제 (확인됨)

| 조건 | INT 8 줄 수 | 전체 줄 수 |
|---|---|---|
| `REPIU_TIMER_INJECT_LOG=1` (120초) | 716 | 1159 |
| 기본값 (150초) | **0** | 402 |

게이팅이 양방향으로 동작함을 대조 실험으로 확인. env 전달은 자식 로더가
`Win32 requested execution backend: aot-dynamic`을 수신하는 것으로 검증.

### 2. 입력 폴링 및 press/release (확인됨)

합성 키 입력('Q' = P1-UpLeft) 주입 결과:

```
[repiu-input] polling started port=0x02A8 value=0xFF
[repiu-input] polling started port=0x02A9 value=0xFF
[repiu-input] polling started port=0x02AA value=0xFF
[repiu-input] P1-UpLeft      PRESSED  port=0x02A8 value=0xFE
[repiu-input] P1-UpLeft      released port=0x02A8 value=0xFF
```

* 세 입력 포트 모두 게스트가 폴링함.
* active-low 폴라리티 **실측 확인**: 누름 시 bit0 clear(`0xFF`→`0xFE`),
  뗌 시 복귀(`0xFE`→`0xFF`). MAME `IP_ACTIVE_LOW` 사양과 일치.
* press/release 1회당 정확히 1줄씩만 출력 — 매 프레임 폴링에도 도배 없음.

### 3. Task 327 수정의 간접 증명 (확인됨)

`polling started` 이후에 발생한 press/release가 관측됐다는 사실 자체가
Task 327 수정의 동작 증거다. 기존 NOP 패치 코드였다면 최초 1회 read 직후
IN 명령이 NOP으로 죽어 이후 어떤 전이도 관측될 수 없었다.

The observed transitions *after* the first poll are themselves proof of the
Task 327 fix: under the previous NOP-patching code the IN instruction would
have been dead after the first read, so no transition could ever appear.

## 관찰된 부수 사실 (Observed, 추가 확인 필요)

* 입력 폴링은 INT 8 타이머 주입이 시작된 직후에 개시된다(로그 line 301→303).
  게임의 입력 폴링이 타이머 인터럽트 구동임을 시사한다.
* 폴링 개시까지 약 60~90초가 걸린다. 짧은 구동(25초)에서는 입력 포트를
  전혀 읽지 않으므로, 입력 관련 검증은 충분히 긴 구동이 필요하다.
* attempt 종료 시 로더가 출력하는 port I/O 요약은 타임아웃 강제 종료 경로에서
  출력되지 않는다(기존 알려진 제약).
