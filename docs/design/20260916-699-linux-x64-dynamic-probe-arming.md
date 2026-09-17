# Task 699 — Linux x64 dynamic-only execution probe arming

## 목적

Task 613은 `REPIU_EXECUTION_PROBE_OFFSET` 대상이 초기 AOT map에 없더라도 dynamic
append 직후 sentinel을 설치하도록 구현했습니다. 그러나 현재 초기화 코드는 최초
`InstallAotProbeSentinel` miss를 설정 오류처럼 처리해
`execution_probe_configured=false`로 바꿉니다. 그 결과 dynamic-only guest 주소인
`0x010F928B`에서는 후속 설치 코드가 도달 불가능합니다.

## 설계

* 유효한 환경 변수는 초기 map 설치 성공 여부와 분리해 configured 상태를 유지합니다.
* 초기 map에 entry가 있으면 기존과 같이 즉시 sentinel을 설치합니다.
* 초기 miss이면 guest 실행을 변경하지 않고 armed 상태로 기다립니다. dynamic append가
  같은 guest 주소의 active exact entry를 게시하면 기존
  `InstallAotProbeSentinelInLatestAppend`가 sentinel을 설치합니다.
* AOT breakpoint를 guest 주소로 역매핑한 직후 execution probe snapshot을 기록합니다.
  snapshot 동안만 context EIP를 guest 주소로 표시하고 즉시 원래 cache EIP로
  복원합니다. 따라서 x64 비동일 명령이 compatibility fail-closed되더라도 레지스터
  증거는 남고, dispatch 결정은 바뀌지 않습니다.
* 기존 generic probe hit 처리, 원래 cache byte 복원, 출력 형식은 변경하지 않습니다.
* 이 기능은 opt-in 진단이며 기본 실행 경로에는 영향을 주지 않습니다.

## 검증

`REPIU_EXECUTION_PROBE_OFFSET=0x000F928B` 실제 실행에서 configured/hit가 true가 되고
dynamic install line과 register snapshot이 출력되는지 확인합니다. Linux x64 core
probe 27개 그룹도 다시 실행합니다.

## English

### Purpose

Task 613 implemented sentinel installation after a dynamic append when a
`REPIU_EXECUTION_PROBE_OFFSET` target is absent from the initial AOT map. The
current initialization path, however, treats the first
`InstallAotProbeSentinel` miss as a configuration error and clears
`execution_probe_configured`. This makes the later installation path
unreachable for a dynamic-only guest address such as `0x010F928B`.

### Design

* Keep a valid environment request configured independently of initial-map
  installation success.
* Preserve immediate sentinel installation when the initial map has an entry.
* On an initial miss, leave guest execution unchanged and keep the probe armed.
  The existing `InstallAotProbeSentinelInLatestAppend` installs it when a
  dynamic append publishes an active exact entry for the guest address.
* Record the execution-probe snapshot immediately after reverse-mapping an AOT
  breakpoint. Temporarily present the guest address as context EIP only while
  recording, then restore the original cache EIP. This preserves register
  evidence even when x64 compatibility fails closed before the generic probe
  path, without changing the dispatch decision.
* Do not change the existing generic probe-hit path, original-byte restoration,
  or output format.
* This remains opt-in diagnostics and does not affect the default path.

### Verification

Run real `pumpit2a` with `REPIU_EXECUTION_PROBE_OFFSET=0x000F928B` and confirm a
dynamic installation line, configured/hit true, and a register snapshot. Run
all 27 Linux x64 core-probe groups again.
