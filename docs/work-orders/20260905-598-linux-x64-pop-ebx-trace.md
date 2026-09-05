# 작업 지시 20260905-598 — Linux x64 AOT `POP EBX` trace 출력

설계: [20260905-598](../design/20260905-598-linux-x64-pop-ebx-trace.md)

## 목표

terminal Linux x64 fault 이전에 `0x010F0233` `POP EBX`의 입력과 직후 EBX를
stderr에 기록하여 Task 597의 `EBX=0` upstream 원인을 분리합니다.

## 작업

1. `ThreadContext`에 execution trace immediate-log opt-in 상태를 추가합니다.
2. `REPIU_EXECUTION_TRACE_LOG=1`을 읽고, trace configured일 때만 활성화합니다.
3. `RecordExecutionTrace` capture 직후 EIP/ESP/stack/EAX/EBX/EDX/EFLAGS를
   host error stream에 출력합니다.
4. Linux x64 build와 core probe를 실행합니다.
5. 두 sentinel trace로 `pumpit2a`를 실행하고 관측값을 analysis와 작업 로그에
   기록합니다.

## 완료 기준

* 기본 실행은 immediate trace line을 출력하지 않습니다.
* opt-in trace line은 terminal fault 전 stderr에 남습니다.
* `POP EBX` 입력/직후 EBX 또는 sentinel 설치·도달 제한이 명확히 기록됩니다.
* 설계, 작업 지시, 작업 로그, analysis가 갱신됩니다.

## English

# Work order 20260905-598 — Linux x64 AOT `POP EBX` trace output

Design: [20260905-598](../design/20260905-598-linux-x64-pop-ebx-trace.md)

## Objective

Record the input to `0x010F0233` `POP EBX` and EBX immediately afterward on
stderr before the terminal Linux x64 fault, separating the upstream cause of
Task 597's `EBX=0`.

## Work

1. Add execution-trace immediate-log opt-in state to `ThreadContext`.
2. Read `REPIU_EXECUTION_TRACE_LOG=1` and enable it only when trace is configured.
3. Immediately write EIP/ESP/stack/EAX/EBX/EDX/EFLAGS after each trace capture.
4. Run the Linux x64 build and core probe.
5. Run `pumpit2a` with two trace sentinels and record observations in analysis
   and the work log.

## Completion criteria

* Default execution emits no immediate trace line.
* Opt-in trace lines survive on stderr before terminal fault.
* The `POP EBX` input/after-state, or a sentinel install/reachability limit, is
  recorded clearly.
* Design, work order, work log, and analysis are updated.
