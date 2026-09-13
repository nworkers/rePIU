# 작업 지시서: Linux x64 마지막 정상 signal 경계 추적

## 한국어

### 배경

Task 672의 경계 측정에서 `entry_rsp`와 `cache_rsp`는 high address이고 `thunk_rsp`만 low address였다. 공통 re-entry 경로가 원본 guest instruction을 실제로 재개했는지 확인하려면 fault 직전 마지막 정상 signal의 native stack과 guest 상태가 필요하다.

### 구현 순서

1. Linux fault handler에 opt-in last-resumed snapshot 저장소를 추가한다.
2. callback이 `kResume`를 반환한 signal에 대해서만 snapshot을 갱신한다.
3. unhandled fault report에 마지막 snapshot과 최초 저주소 snapshot을 출력한다.
4. WSL Linux x64 debug build를 수행하고, 내부 timeout은 끈 bounded run으로 fault 직전 값을 수집한다.
5. 결과를 작업 로그와 누적 analysis 문서에 반영한다.

### 완료 조건

* Linux x64 build가 성공한다.
* snapshot이 현재 fault 자체가 아니라 마지막 정상 resume을 가리킨다.
* 최초 저주소 snapshot이 있으면 해당 signal의 host/guest 경계를 가리킨다.
* trace 비활성 상태에서 기본 실행 의미가 바뀌지 않는다.
* 결과가 특정 EIP 예외가 아닌 공통 signal 경계 증거로 기록된다.

## English

### Background

Task 672 measured a high native address at guest entry and before the cache call, but a low address at ReturnThunk entry. The last successfully resumed signal must be sampled to determine whether a shared re-entry path resumed original guest bytes and changed the host stack.

### Implementation order

1. Add an opt-in last-resumed snapshot to the Linux fault handler.
2. Update it only after a callback returns `kResume`.
3. Print both snapshots in the unhandled-fault report.
4. Build the Linux x64 debug target through WSL and run with internal timeouts disabled and an external bound.
5. Record the result in the work log and cumulative analysis.

### Done when

* The Linux x64 build succeeds.
* The snapshot describes the last successful resume rather than the current fault, and records the first low-`RSP` resume when present.
* Disabled tracing does not alter the default execution semantics.
* The evidence describes a shared signal boundary, not a guest-EIP-specific exception.
