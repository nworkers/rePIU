# 진단 진행 기반 polling 설계

## 배경

`piu_1st`에 실제 DOS environment block을 제공한 뒤, guest는 긴 environment entry를 스캔한다. 기존 진단 실행은 고정 polling 반복 횟수만 사용하므로 guest가 계속 진행 중이어도 `chdir`/`open` 이후 지점까지 도달하기 전에 timeout으로 종료될 수 있다.

## 설계

Win32 trampoline의 진단 polling은 두 가지 조건을 함께 사용한다.

* 조용한 반복 한도: 진행 관측값이 변하지 않는 상태가 일정 반복 이상 지속되면 timeout으로 본다.
* wall-clock 한도: 진행이 계속 있더라도 caller가 넘긴 timeout millisecond를 넘기면 timeout으로 본다.

진행 관측값은 guest thread가 갱신하는 atomic counter만 사용한다. 상세 environment entry 이름이나 offset 같은 비원자 필드는 기존처럼 guest thread가 멈춘 뒤 attempt로 복사한다. 이렇게 하면 진행 판단 중 host thread가 상세 진단 필드를 직접 읽지 않는다.

## 관측값

다음 값을 `Win32MinimalExecutionAttempt`에 복사해 loader가 출력한다.

* diagnostic poll iteration count
* diagnostic progress count
* diagnostic quiet iteration count

## 범위 밖

* PSP/environment selector 모델은 이번 작업에서 만들지 않는다.
* single-step opcode handler를 추가하지 않는다.
* guest 로직은 수정하지 않는다.

# Diagnostic Progress Polling Design

## Background

After `piu_1st` receives a real DOS environment block, the guest scans long environment entries. The previous diagnostic execution used only a fixed polling iteration count, so it could time out before reaching later `chdir`/`open` points even while the guest was still making progress.

## Design

The Win32 trampoline diagnostic polling uses two conditions together.

* Quiet iteration limit: if the progress observation does not change for a fixed number of polling iterations, treat the run as timed out.
* Wall-clock limit: even if progress continues, stop when the caller-provided timeout in milliseconds is reached.

The progress observation uses only atomic counters updated by the guest thread. Detailed non-atomic fields such as environment entry names and offsets are still copied only after the guest thread is stopped. This avoids reading detailed diagnostic fields from the host polling loop while the guest is running.

## Observations

The following values are copied into `Win32MinimalExecutionAttempt` and printed by the loader.

* diagnostic poll iteration count
* diagnostic progress count
* diagnostic quiet iteration count

## Out Of Scope

* Do not implement the PSP/environment selector model in this task.
* Do not add new single-step opcode handlers.
* Do not modify guest logic.
