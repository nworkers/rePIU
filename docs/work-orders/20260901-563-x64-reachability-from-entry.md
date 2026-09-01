# 20260901-563 진입점 기준 도달 가능성 측정 작업 지시서

## 한국어

### 목적

x64에서 무엇이 실행을 막는지 측정으로 정합니다. 설계는
[20260901-563](../design/20260901-563-x64-reachability-from-entry.md)입니다.

### 작업

- census에 진입점 기준 도달 가능성 walk를 추가한다.
- 완결된 block만 따라가고, call은 target과 fallthrough 둘 다, return은 따라가지 않는다.
- 체인이 멈춘 지점을 record kind와 이유별로 집계한다.
- 진입 주소와 첫 정지 주소를 찍는다.

### 검증

Linux x64에서 census를 실행해 도달 가능 block 수와 정지 지점을 기록합니다. i386과
Win32는 census를 빌드해 회귀로 확인합니다.

## English

### Objective

Decide by measurement what blocks execution on x64. The design is
[20260901-563](../design/20260901-563-x64-reachability-from-entry.md).

### Work items

- Add a reachability walk from the entry to the census.
- Follow only complete blocks; a call queues target and fallthrough, a return queues
  nothing.
- Tally where chains stopped, by record kind and reason.
- Print the entry address and the first stopping address.

### Verification

Run the census on Linux x64 and record the reachable-block count and where chains stop.
Build the census on i386 and Win32 as regressions.
