# 작업 로그: 진단 이름 뒤에 숨은 끄기 스위치와 DOS/DPMI 실험 두 개 삭제 (issue #24)

설계: `docs/design/20261009-i024-retire-hidden-toggles.md`
작업 지시: `docs/work-orders/20261009-i024-retire-hidden-toggles.md`

## 한 일

* **Glide draw 진입점**: `REPIU_GLIDE_DRAW_ENTRY_POINTS`와 꺼졌을 때 그리지 않고 돌아가던
  네 블록을 지웠다(`linexe_glide_boundary.cpp`, 54줄 삭제). 그 블록들이 기록하던 issue 사유
  문자열(`draw-point-noop` 등)을 읽는 곳은 없었다.
* **timer tick backlog**: bool 정책 분기, `ResolveTimerTickBacklogEnabled`·
  `TimerTickBacklogEnabled`, `RecordTimerTicksDue`의 `already_pending`·`backlog_enabled` 인자,
  `RecordTimerTickInjected`의 `backlog_enabled` 인자를 지웠다. backlog 경로는 `coalesced`를
  올리지 않으므로 `coalesced_total`·`coalesced_in_gate_total`, snapshot의 `backlog_enabled`,
  CD census의 두 열을 지웠다. 최종 로그는
  `timer tick delivery due/injected/dropped/deferred/max-backlog/remaining`과
  `timer tick in-gate due` 두 줄이 됐다. 호출부(trampoline, live snapshot)에서 이제 쓰지 않는
  `timer_interrupt_pending` 읽기 하나가 빠졌다. probe는 bool 정책 단언 세 묶음(policy,
  coalescing, already_pending)을 지우고, 게이트 귀속 단언을 backlog 기준으로 다시 썼다.
* **JAMMA 스냅샷**: `REPIU_JAMMA_SNAPSHOT` 읽기만 지웠다. `REPIU_JAMMA_SNAPSHOT_US`는 남는다.
* **PharLap 경로 실험**: `Dos4gwPharlapMemoryPathProbeEnabled`와 `AH=30h` 두 처리기의
  `0x44580000` 덧붙임, 그 함수만 쓰던 `host_environment.h` include를 지웠다.
* **1E7F**: `INT 31h AX=1E7Fh` 블록을 지웠다. 게임 실행 없이 판단했다 — Task 606 작업 로그와
  `EXE_DESIGN`이 이 호출을 스택 손상의 결과로 확정하고, 수정 뒤 기본 실행에서 호출이
  사라졌다고 기록한다.
* **스크립트·문서**: `task366_timer_tick_delivery.ps1` 삭제, `task414` 정규식 수정.
  ARCHITECTURE 타이머 tick 절, CD census 가이드, frontier 토글 표, I/O 포트 명세, README,
  환경 변수 목록, TODO를 고쳤다.

### 함께 찾은 것

* **frontier 토글 표**는 #20·#22에서 지운 변수 5개를 아직 싣고 있었고, 그 뒤 승격된
  `REPIU_AOT_DBT_SUPERBLOCK`과 Task 432에서 기본값이 바뀐 `REPIU_TIMER_TICK_BACKLOG`를 OFF로
  적고 있었다. 현재 상태로 다시 썼다. (issue 본문에 적은 "6개"는 세기 착오였다.)
* **#22의 probe 기대값**: #22가 `kAotReentryNativeSpan` 버킷을 지웠는데
  `execution_time_profile_probe`는 단일 스텝 버킷을 26개 중 25번으로 기대해, Win32
  `repiu_aot_probe` 체인이 `execution_time_profile_all=false`에서 멈췄다. #22는 Linux에서만
  검증해 aot_probe를 빌드하지 못했다. 기대값을 25개 중 24번으로 고쳤다(커밋 `28beeac`, #22).

## 검증

* **빌드**: Win32 Release(`build/win32_x86_debug`) 오류 0. Linux x64 Release(WSL,
  `build/linux_x64_release`) 통과, 경고는 손대지 않은 `fault_handler_arch.cpp`의 기존 것 하나.
* **timer tick probe**(`repiu_aot_probe --jamma-input-timeline`): draining·capping·clearing·
  deferral·gate_attribution·inert 모두 true, 종료 코드 0.
* **aot_probe 전체 체인**(`MASTER/PIU_1ST/PIU/PIU.EXE`): 위 #22 기대값 수정 뒤 끝까지
  통과(종료 코드 0, 539줄). `cache_executable=false`는 v0.0.207에도 있는 값이다. v0.0.207
  릴리스 바이너리는 #22에서 지운 `dbt_indirect_dispatch` 단계에서 먼저 멈추므로, 비교는 그
  앞 70줄까지만 가능했고 시간 값 외에는 같았다.
* **core probe**: Linux x64 39개 모두 통과. Win32는 37개 중 `stack_bridge` 하나 실패 —
  열린 issue #8(Release에서의 stack_bridge probe)의 기존 실패다.
* **Glide probe**: `repiu_glide_issue_probe`, `repiu_glide_render_probe` 통과.
* **변수 읽기**: `src/`·`include/`에 6개 변수 문자열이 없다.
* **게임 실행**(사용자 확인 뒤, Win32 Release, pumpit2a 60초, `scripts/survey_romsets.sh`,
  NVRAM은 실행마다 임시 디렉터리): 기준은 v0.0.207 릴리스. 두 쌍을 순서를 바꿔 돌렸다.

  | 실행 | 프레임 | fault | 자산 열기 | tick due/injected/dropped | max-backlog |
  |---|---:|---:|---:|---|---:|
  | 1쌍 새 빌드 | 3,051 | 0 | 21 | 13,527 / 13,444 / 83 | **64** |
  | 1쌍 v0.0.207 | 3,184 | 0 | 23 | 14,079 / 14,053 / 26 | 20 |
  | 2쌍 v0.0.207 | 3,202 | 0 | 23 | 14,120 / 14,098 / 22 | 19 |
  | 2쌍 새 빌드 | 3,203 | 0 | 23 | 14,121 / 14,099 / 22 | 19 |

  1쌍의 새 빌드는 8초와 27초 지점의 끊김이 더 깊어(6.8·20.7fps 대 27.6·51.2fps) backlog가
  상한에 닿았고 약 1초 뒤처져 마지막 자산 두 개를 열기 전에 끝났다. 2쌍에서는 초당 fps
  열까지 거의 같았으므로 실행 편차로 판정했다. 장면 순서는 네 실행 모두 같고, 새 최종 로그
  줄(`timer tick delivery due/injected/dropped/deferred/max-backlog/remaining`)이 의도대로
  찍혔다.
* **확인하지 못한 것**: Linux i386 빌드.

## Linux 검증 (2026-10-10)

Ubuntu 26.04.1(RTX 4090, 실행은 x11)에서 main `2809668`(v0.0.213과 사이트 링크 커밋)으로
위에서 남긴 항목을 확인했다. 이 작업 이후의 #25·#30·#31·#34·#37이 함께 들어간 트리다.

* **빌드**: Linux i386(direct)·x64(cache) Release(`build/verify-i386`, `build/verify-x64`),
  모든 기본 타깃 통과. 경고는 원래 있던 것뿐이다(`g_repiu_active_thread_context`, x64의
  `fault_handler_arch.cpp`, i386의 minimp3).
* **core probe**: 두 아키텍처 모두 `core_probe_failures=0`, 종료 코드 0. i386의
  `shutdown_recovery_policy_wide_pointer=false`는 #22 기준선에도 있는 값이다.
* **게임 실행**: #37 작업 로그의 Linux 검증 절에 적은 pumpitea·pumpit8 실행에서 새 최종 로그 줄
  `timer tick delivery due/injected/dropped/deferred/max-backlog/remaining`이 두 아키텍처
  모두 찍혔다(예: x64 pumpitea 90초 `20842/20780/62/5807/10/0`).

---

# Work log: retire the hidden kill switches and two DOS/DPMI experiments (issue #24)

**Done.** The Glide draw entry point switch and its four non-drawing blocks went; nothing read
their issue reason strings. The timer tick backlog lost its boolean policy branch, resolver,
the `already_pending`/`backlog_enabled` arguments, the always-zero `coalesced` counters, the
snapshot's `backlog_enabled`, and the two CD census columns; the final log is now
`timer tick delivery due/injected/dropped/deferred/max-backlog/remaining` and
`timer tick in-gate due`, and the call sites lost a `timer_interrupt_pending` read they no
longer need. The probe dropped the policy, coalescing and already-pending groups and rewrote
gate attribution on the backlog. Only the `REPIU_JAMMA_SNAPSHOT` read went; the interval stays.
The PharLap experiment went from both `AH=30h` handlers with its now unused include. The
`AX=1E7Fh` block went without a game run, since Task 606 and `EXE_DESIGN` record the call as the
product of stack corruption that the default run no longer makes. `task366` was deleted, the
`task414` regex fixed, and ARCHITECTURE, the census guide, the frontier toggle table, the I/O
port specification, README, the inventory and the TODO updated.

**Found along the way.** The frontier toggle table still listed five variables deleted in
#20/#22 and showed both the later-promoted `REPIU_AOT_DBT_SUPERBLOCK` and the Task 432
default-on `REPIU_TIMER_TICK_BACKLOG` as off; it now shows the current state (the issue text's
"six" was a miscount). And #22's deletion of `kAotReentryNativeSpan` left
`execution_time_profile_probe` expecting the single-step bucket at 25 of 26, which stopped the
Win32 `repiu_aot_probe` chain; #22 verified on Linux only and could not build aot_probe. The
expectation is now 24 of 25 (commit `28beeac`, #22).

**Verification.** Win32 Release build with no errors; Linux x64 Release build (WSL) with only a
pre-existing warning in an untouched file. The timer tick probe group passes. The full aot_probe
chain on `MASTER/PIU_1ST/PIU/PIU.EXE` now runs to the end (exit 0); its
`cache_executable=false` is also in v0.0.207, whose release binary stops earlier at the
indirect dispatch probe #22 deleted, so only the first 70 lines compare — identical apart from
timings. Core probe: all 39 pass on Linux x64; on Win32 only `stack_bridge` fails, the existing
failure tracked in open issue #8. Both Glide probes pass. No source reads the six variables.
**Game run** (with the user's go-ahead; Win32 Release, pumpit2a for 60 s via
`scripts/survey_romsets.sh`, NVRAM in a fresh temporary directory per run, against the v0.0.207
release, two pairs in alternating order): no faults anywhere and the same scene sequence in all
four runs; the new final-log line prints as intended. In the first pair the new build's hitches
at 8 s and 27 s ran deeper (6.8 and 20.7 fps against 27.6 and 51.2), the backlog reached its cap
of 64 (83 dropped against 26), and the run ended about a second behind, before the last two asset
opens. The second pair matched almost exactly (frames 3,202/3,203, ticks 14,120/14,121, 22
dropped and a peak backlog of 19 on both sides, 23 opens each), so the first pair's gap is
run-to-run variation. **Not done:** a Linux i386 build.

**Linux verification (2026-10-10).** On Ubuntu 26.04.1 (RTX 4090, running on x11), against main
`2809668` (v0.0.213 plus the site link commits, so #25, #30, #31, #34 and #37 are in the tree):
Linux i386 (direct) and x64 (cache) Release builds (`build/verify-i386`, `build/verify-x64`) of
every default target pass with only pre-existing warnings (`g_repiu_active_thread_context`, x64's
`fault_handler_arch.cpp`, i386's minimp3). The core probe reports `core_probe_failures=0` and exits
0 on both; i386's `shutdown_recovery_policy_wide_pointer=false` is also in the #22 baseline. The
pumpitea and pumpit8 runs recorded in #37's Linux verification print the new
`timer tick delivery due/injected/dropped/deferred/max-backlog/remaining` line on both
architectures (x64 pumpitea, 90 s: `20842/20780/62/5807/10/0`).
