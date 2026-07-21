# 20260721-328-input-log-observability

## 개요 (Overview)

INT 8 타이머 주입 로그가 콘솔을 가득 채워 다른 진단을 가리는 문제를 해결하고,
키 입력이 실제로 게스트에 전달되는지 눈으로 확인할 수 있는 입력 로그를
추가합니다.
Silence the INT 8 timer-injection log that floods the console and hides other
diagnostics, and add input logging that makes it visible whether key presses
actually reach the guest.

관련 설계 / Related: `docs/design/20260721-327-jamma-input-poll-eip-advance.md`

---

## 변경 대상 (Scope)

* `src/platform/win32/execution/execution_trampoline.cpp`
  - `Injected INT 8` 로그를 `REPIU_TIMER_INJECT_LOG` env 게이팅으로 기본 비활성화.
    (기존 `REPIU_GLIDE_TEX_DIAG` 방식과 동일한 idiom)
* `src/platform/win32/io/port_io_emulator.cpp`
  - 포트별 비트 이름표(`JammaBitName`) 추가.
  - `LogJammaInputTransition` 추가: 포트 최초 폴링 1회 로그 +
    **엣지 트리거** press/release 로그.

---

## 로그 설계 근거 (Log Design Rationale)

게스트는 입력 포트를 매 프레임 폴링하므로, 읽을 때마다 로그를 찍으면 INT 8과
똑같은 홍수가 재발한다. 따라서 두 종류만 출력한다.
The guest polls the input ports every frame, so logging every read would
reproduce the same flood. Only two kinds of lines are emitted.

| 로그 / Line | 시점 / When | 빈도 / Volume |
|---|---|---|
| `polling started port=0x02A8 value=0xFF` | 포트별 최초 read 1회 | 최대 3줄 |
| `P1-UpLeft PRESSED` / `released` | 비트 상태가 바뀔 때만 | 사람 입력 횟수만큼 |

`polling started`가 필요한 이유: 로그가 조용할 때 "키 매핑이 깨진 것"과
"게스트가 아직 폴링을 시작하지 않은 것"을 구분할 수 없기 때문이다.
`polling started` disambiguates a broken key mapping from a guest that has not
begun polling yet.

---

## 완료 조건 (Acceptance)

1. 기본 구동에서 `Injected INT 8` 로그가 출력되지 않는다.
2. `REPIU_TIMER_INJECT_LOG=1`로 재활성화된다.
3. 입력 포트 폴링 시작이 포트별 1회 보고된다.
4. 키 press/release가 각각 1줄씩 보고되고, 폴링 반복으로 도배되지 않는다.
