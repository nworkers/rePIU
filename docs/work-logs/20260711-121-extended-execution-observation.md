# 장시간 실행 관찰 작업 로그

## 변경

* loader가 `REPIU_EXECUTION_TIMEOUT_MS`의 유효한 양의 32비트 값을 실행 제한으로 사용하도록 했다.
* 환경 변수가 없거나 잘못되면 기존 1,000ms 기본값을 유지한다.
* supervisor가 전체 제한에서 1,000ms 종료 여유를 뺀 값을 자식에게 전달하도록 했다.

```mermaid
flowchart LR
    SUP["Supervisor 15,000ms"] -->|"environment"| LOAD["Loader 14,000ms"]
    LOAD --> GUEST["Guest execution"]
    GUEST -->|"9.7s"| INT3["+0xF3438 INT 3"]
    INT3 --> CLEAN["Orderly child collection"]
```

## 검증

* `scripts/build_win32_x86.bat`: 성공, `build/win32_x86_debug/Debug`에서 loader와 supervisor 재빌드.
* `repiu_loader_win32.exe dos4gw_hello`: 성공, `Hello, world!`, 기본 제한 1,000ms 확인.
* `repiu_supervisor_win32.exe piu_1st 15000`: loader 제한 14,000ms 확인.
* PIU 실행은 약 9.7초 동안 약 118.5만 dispatch까지 진행했으며 supervisor 강제 종료 없이 회수됨.
* 새 경계는 object 2 `+0xF3438`의 `INT 3`; `PIU.BIN` I/O는 그 전에 성공함.

## 결론

관찰 시간 확장은 정상 동작하며 이전 1초 관찰보다 상당히 뒤의 실행 경계를 확인했다. 다음 작업에는 `+0xF3438`의 caller와 직전 분기 조건 분석이 필요하다.

# Extended Execution Observation Work Log

## Changes and validation

The loader now accepts a valid positive 32-bit `REPIU_EXECUTION_TIMEOUT_MS`, retaining the 1,000ms default for absent or invalid input. The supervisor passes a child deadline with a 1,000ms shutdown margin.

The Win32 x86 Debug build and standalone DOS/4GW hello execution succeeded. A PIU run under a 15,000ms supervisor deadline used a 14,000ms loader deadline, progressed to roughly 1.185 million dispatches over about 9.7 seconds, and exited without forced supervisor termination at the `INT 3` at object 2 `+0xF3438`. `PIU.BIN` I/O had succeeded before that frontier. The next task is caller and branch-condition provenance analysis for this breakpoint.
