# 작업 로그: Linux x64 릴리스 CI의 간헐적 `host_thread_interrupt=false` (issue #12)

작업 지시: `docs/work-orders/20261010-i012-host-thread-interrupt-probe-flake.md`

## 확인한 것

* 릴리스 워크플로 기록: v0.0.201·202·208·214가 실패, 나머지는 통과. v0.0.214(실행 38041054474)도
  Linux x64 잡의 core probe에서 `host_thread_interrupt=false` 하나로 실패했고 다른 probe는 모두 통과했다.
  #12의 재실행 기록대로 같은 커밋이 다시 돌리면 통과하는 간헐적 실패다.
* 실패 줄은 일곱 가지 주장을 한 값으로 묶어 어느 것이 틀렸는지 알 수 없었다. 그중 "표본이 움직임"은
  세 명령어짜리 루프 하나에서 33번 연속 같은 RIP가 나오면 실패한다. 인터럽트는 명령어 경계에서 받히고
  긴 지연 명령 뒤에 몰리는 경향(skid)이 있어 CPU에 따라 같은 주소가 반복될 수 있다. GitHub runner의 CPU는
  실행마다 다르므로 간헐적 실패 양상과 맞는다(**추정**).
* 이 기계(Ubuntu 26.04.1)에서 루프를 하나로 되돌린 실험: 20회 중 움직이기까지 표본 1개 15회, 2개 2회,
  3개 2회, 7개 1회. 꼬리가 있지만 이 CPU에서 32개를 넘지는 않았다. CI 하드웨어에서의 원인은 아직
  관찰되지 않았다.

## 한 일

* `ProbeInterruptSamplesAndEdits`가 주장별 결과(`InterruptOutcome`)를 돌려주고, probe가
  `host_thread_interrupt_parts answered/one_callback/nonzero/on_target/moved/edit/joined=… move_attempts=N`
  한 줄을 더 출력한다.
* 대상 스레드가 인라인 금지 함수 두 개(`SpinCountingUp`, 반대로 세는 `SpinCountingDown`)를 번갈아
  돈다. 두 함수는 주소가 달라 표본 위치 편향만으로 같은 값이 계속 나올 수 없고, 코드가 달라 MSVC
  /OPT:ICF로 합쳐지지 않는다.

## 검증

| 검증 | 결과 |
|---|---|
| Linux x64 core probe 20회 | 모두 통과, 움직이기까지 표본 1개 18회, 2개 1회, 3개 1회 |
| Linux i386 core probe 10회 | 모두 통과, 1개 8회, 2개 2회 |
| 브랜치에서 `release.yml` 수동 실행 38044904199 | Linux x64·i386 잡 통과, 두 잡 모두 `parts …=1/1/1/1/1/1/1 move_attempts=1` |
| Win32 | PR #49 CI가 빌드(이 파일 포함) |

## 남은 것

* 간헐적 실패였으므로 한 번의 통과로 해결을 단정하지 않는다. 다음 실패가 있으면 새 줄이 어느 주장인지
  말해 준다. 움직임이 아니라 다른 주장이면 이 수정과 별개로 다시 본다.
* v0.0.214 릴리스는 아직 올라가지 않았다(실행 38041054474의 실패한 Linux x64 잡을 다시 돌리면 그 태그의
  코드로 올라간다).

---

# Work log: intermittent `host_thread_interrupt=false` in the Linux x64 release CI (issue #12)

**Found.** The release workflow failed on v0.0.201, .202, .208 and .214 and passed otherwise; v0.0.214 (run
38041054474) failed in the Linux x64 job's core probe on `host_thread_interrupt=false` alone, and, as #12's
rerun record shows, the same commit passes when run again. The failing line folded seven claims into one
value. One of them, "the sample moved", fails when 33 samples of a three-instruction loop all read the same
RIP; interrupts are taken at instruction boundaries and tend to land after long-latency instructions
(skid), so some CPUs may repeat one address, and GitHub's runner CPUs vary between runs, which fits the
intermittent pattern (**inferred**). On this machine (Ubuntu 26.04.1), the loop put back to a single
function needed 1 sample 15 times in 20, 2 twice, 3 twice and 7 once: a tail, never past 32 here; the cause
on CI hardware has not been observed.

**Done.** `ProbeInterruptSamplesAndEdits` returns each claim (`InterruptOutcome`) and the probe prints
`host_thread_interrupt_parts answered/one_callback/nonzero/on_target/moved/edit/joined=… move_attempts=N`.
The target alternates between two out-of-line functions (`SpinCountingUp` and the opposite-counting
`SpinCountingDown`), at two addresses, so a biased sample position alone cannot repeat one value, and with
different code, so MSVC /OPT:ICF cannot merge them.

**Verified.** Linux x64 core probe 20 times: all pass, moving after 1 sample 18 times, 2 once, 3 once. Linux
i386 10 times: all pass, 1 sample 8 times, 2 twice. A manual `release.yml` run on the branch (38044904199):
the Linux x64 and i386 jobs pass, both with `parts …=1/1/1/1/1/1/1 move_attempts=1`. Win32 builds it in PR
#49's CI.

**Left.** An intermittent failure is not settled by one pass; a later failure will name its claim through
the new line, and one other than "moved" gets its own look. v0.0.214 is still unpublished (rerunning the
failed Linux x64 job of run 38041054474 would publish it with that tag's code).
