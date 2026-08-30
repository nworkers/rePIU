# 20260830-528 pumpipx3와 pumpit1 성능 차이 귀속 작업 로그

설계: [20260830-528](../design/20260830-528-pumpipx3-performance-attribution.md)

작업 지시: [20260830-528](../work-orders/20260830-528-pumpipx3-performance-attribution.md)

## 결과 요약

중단된 성능 분석을 현재 작업 브랜치에서 이어서 수행했습니다. Linux i386 Release 빌드는
성공했고, 두 ROM set을 같은 무트레이스 환경에서 다시 실행했습니다. `pumpipx3`의
late drop은 재현되었지만, `INT 21h/AH=2Ch` 누적 호출 수는 late drop 시점에 증가하지
않았습니다. 따라서 이번 단위에서 DOS AH2C를 원인으로 확정하거나 성능 수정은 하지
않습니다.

## 계측 보정

기존 `handled_dos_interrupt_ah_counts`는 모든 HLE interrupt vector의 AH를 합산하는 배열이어서
INT 16h·31h·33h 등의 값이 DOS AH 분포에 섞일 수 있었습니다. 다음 두 카운터를 추가하고
`vector == 0x21`일 때만 갱신했습니다.

* `handled_dos_int21_count`
* `handled_dos_int21_ah_counts[256]`

`[repiu-live-dos]`는 이제 전체 HLE interrupt 처리 수, INT 21h 처리 수, INT 21h 상위 AH를
분리해 출력합니다. 호출별 trace는 추가하지 않으므로 성능 측정 자체의 trace 오버헤드는
늘리지 않습니다.

## 검증 환경과 명령

브랜치는 `perf-pumpipx3-vs-pumpit1-followup`입니다. 다음 빌드가 성공했습니다.

```text
wsl.exe -d Ubuntu-24.04 -- cmake --build /mnt/e/MYWORK/Projects/rePIU/build/linux_i386 --parallel 2
```

실행은 다음 환경을 두 타이틀에 동일하게 적용했습니다. 실행 로그는 저장소 밖 임시 파일에
기록했습니다.

```text
REPIU_STALL_TIMEOUT_MS=0
REPIU_EXECUTION_TIMEOUT_MS=60000
REPIU_GLIDE_SWAP_INTERVAL=0
REPIU_GLIDE_FRAME_RATE_LOG=1
REPIU_EXECUTION_TIME_PROFILE=1
REPIU_LIVE_PROFILE_INTERVAL_MS=10000
```

실제 실행 형식은 다음과 같습니다.

```text
wsl.exe -d Ubuntu-24.04 --cd /mnt/e/MYWORK/Projects/rePIU -- bash -c 'exec env REPIU_STALL_TIMEOUT_MS=0 REPIU_EXECUTION_TIMEOUT_MS=60000 REPIU_GLIDE_SWAP_INTERVAL=0 REPIU_GLIDE_FRAME_RATE_LOG=1 REPIU_EXECUTION_TIME_PROFILE=1 REPIU_LIVE_PROFILE_INTERVAL_MS=10000 ./build/linux_i386/repiu pumpipx3 >/dev/null 2>/mnt/c/Users/nworkers/AppData/Local/Temp/repiu_followup_pumpipx3_corrected_60.err'
wsl.exe -d Ubuntu-24.04 --cd /mnt/e/MYWORK/Projects/rePIU -- bash -c 'exec env REPIU_STALL_TIMEOUT_MS=0 REPIU_EXECUTION_TIMEOUT_MS=60000 REPIU_GLIDE_SWAP_INTERVAL=0 REPIU_GLIDE_FRAME_RATE_LOG=1 REPIU_EXECUTION_TIME_PROFILE=1 REPIU_LIVE_PROFILE_INTERVAL_MS=10000 ./build/linux_i386/repiu pumpit1 >/dev/null 2>/mnt/c/Users/nworkers/AppData/Local/Temp/repiu_followup_pumpit1_corrected_60.err'
```

## 측정 결과

| 항목 | pumpipx3 | pumpit1 |
| --- | ---: | ---: |
| 종료 표본 (`frames / span_ms`) | `1497 / 56786` | `2207 / 57964` |
| 종료 표본 평균 FPS | 약 `26.4` | 약 `38.1` |
| live #5 window frames | `50` | `301` |
| live #5 cycles/frame | `742098553` | `123105111` |
| live #5 VEH 비율 | `80.88%` | `59.53%` |
| live #5 DOS 비율 | `0.59%` | `2.61%` |
| live #5 AOT boundary | `388247` | `27986` |
| live #5 AOT reentry | `1581505` | `251583` |
| live #5 상위 boundary opcode | `CD=359779` | `8A=6748` |
| live #5 DOS | `handled=359779`, `int21=356763`, `AH2C=356476` | `handled=1601`, `int21=906`, `AH3B=336` |

`pumpipx3` 프레임 로그는 약 35.6초에 `4.8 FPS`를 기록한 뒤 `4.8~5.1 FPS`로
유지됐습니다. 반면 `pumpit1`은 같은 60초 실행에서 고정 5 FPS 상태로 내려가지 않았습니다.
다만 이전 실행에서도 절대 FPS와 late-drop 시점이 변했으므로, 위 종료 FPS를 안정적인
벤치마크 수치로 해석하지 않습니다.

## 판정

### 확인됨

* `pumpipx3`는 `CD` 경계와 VEH 비용이 `pumpit1`보다 훨씬 큰 실행 형태를 보였습니다.
* `pumpipx3`의 `AH2C=356476`은 60초 live 표본 #1~#5에서 그대로였고, late drop은 그 뒤에
  발생했습니다.
* `pumpit1`의 INT 21h 상위 AH는 파일/디스크 관련 `3B/4A/44/3F`였고 AH2C는 관측되지
  않았습니다.
* 전체 빌드와 계측 실행은 종료 이유 `timeout` 및 shutdown dump까지 도달했습니다.

### 추정

`pumpipx3`의 프레임 차이는 DOS 서비스 본체 단독 비용보다 원본 게스트 timing과 HLE/AOT
경계 왕복이 결합된 결과일 가능성이 큽니다. 이번 실행에서 late drop 부근의 AOT `FF`
표본 및 MP3 초기화는 다음 조사 후보이지만, 인과관계는 아직 입증되지 않았습니다.

### 미확정 및 다음 작업

late drop을 만든 게스트 장면/상태 전환과 AOT `FF` 경로의 정확한 관계는 미확정입니다.
다음 분석은 late drop 직전·직후의 원본 게스트 상태와 boundary opcode `FF` 경로를 좁혀야
합니다. DOS AH2C 호출 제거, timing 우회, 원본 EXE 패치 같은 추측성 수정은 보류합니다.

## 변경 파일과 보존 사항

Task 528의 live opcode/AH 계측과 관련 문서를 현재 브랜치에 남겼습니다. 기존 사용자 산출물인
`pass1.txt`~`pass6.txt`, `repiu.exe`, `repiu150.exe`는 커밋 대상에서 제외합니다.

---

# 20260830-528 Work Log — Performance Attribution for pumpipx3 vs pumpit1

Design: [20260830-528](../design/20260830-528-pumpipx3-performance-attribution.md)

Work order: [20260830-528](../work-orders/20260830-528-pumpipx3-performance-attribution.md)

## Summary

The interrupted performance analysis was resumed on the task branch. The Linux i386 Release
build succeeded, and both ROM sets were run again under the same trace-free environment.
`pumpipx3` reproduced a late drop, but its cumulative `INT 21h/AH=2Ch` count did not increase at
the drop. This unit therefore does not identify AH2C as the cause and applies no performance fix.

## Instrumentation correction

The existing `handled_dos_interrupt_ah_counts` aggregates AH across every HLE interrupt vector,
so values from `INT 16h`, `31h`, or `33h` could be mixed into a DOS AH distribution. Two counters
were added and incremented only when `vector == 0x21`:

* `handled_dos_int21_count`
* `handled_dos_int21_ah_counts[256]`

`[repiu-live-dos]` now separates total HLE-interrupt handling, INT 21h handling, and the top INT
21h AH values. No per-call trace is added, so the measurement does not add DOS trace overhead.

## Verification environment and commands

The task branch is `perf-pumpipx3-vs-pumpit1-followup`. The following build succeeded:

```text
wsl.exe -d Ubuntu-24.04 -- cmake --build /mnt/e/MYWORK/Projects/rePIU/build/linux_i386 --parallel 2
```

Both titles used the same settings:

```text
REPIU_STALL_TIMEOUT_MS=0
REPIU_EXECUTION_TIMEOUT_MS=60000
REPIU_GLIDE_SWAP_INTERVAL=0
REPIU_GLIDE_FRAME_RATE_LOG=1
REPIU_EXECUTION_TIME_PROFILE=1
REPIU_LIVE_PROFILE_INTERVAL_MS=10000
```

The logs were redirected to temporary files outside the repository. The exact command shape is
shown in the Korean section above.

## Results

| Metric | pumpipx3 | pumpit1 |
| --- | ---: | ---: |
| Shutdown sample (`frames / span_ms`) | `1497 / 56786` | `2207 / 57964` |
| Shutdown-sample average FPS | about `26.4` | about `38.1` |
| Live #5 window frames | `50` | `301` |
| Live #5 cycles/frame | `742098553` | `123105111` |
| Live #5 VEH share | `80.88%` | `59.53%` |
| Live #5 DOS share | `0.59%` | `2.61%` |
| Live #5 AOT boundary | `388247` | `27986` |
| Live #5 AOT reentry | `1581505` | `251583` |
| Live #5 top boundary opcode | `CD=359779` | `8A=6748` |
| Live #5 DOS | `handled=359779`, `int21=356763`, `AH2C=356476` | `handled=1601`, `int21=906`, `AH3B=336` |

The `pumpipx3` frame log reached `4.8 FPS` at about 35.6 seconds and then stayed around
`4.8–5.1 FPS`. `pumpit1` did not enter a fixed 5 FPS state during the same 60-second run.
Absolute FPS and the late-drop point varied across earlier runs, so the shutdown FPS values are
not treated as a stable benchmark.

## Assessment

### Confirmed

* `pumpipx3` showed a much larger `CD` boundary population and VEH cost than `pumpit1`.
* `pumpipx3`'s `AH2C=356476` remained unchanged across live samples #1–#5, while the late drop
  happened later.
* `pumpit1` was led by INT 21h AH values `3B/4A/44/3F`; AH2C was not observed.
* The full build and instrumented runs reached timeout shutdown and the shutdown dump.

### Inferred

The `pumpipx3` frame difference is more likely an interaction between original guest timing and
HLE/AOT boundary round trips than the DOS service body alone. AOT `FF` samples and MP3
initialization increased near the late drop in this run, but causality is not established.

### Unresolved and next unit

The exact relationship between the guest scene/state transition and the AOT `FF` path remains
unresolved. The next analysis should narrow the original guest state and boundary path immediately
before and after the drop. Speculative AH2C removal, timing bypasses, and original-EXE patches are
deferred.

## Changed files and preserved artifacts

Task 528 live opcode/AH instrumentation and the related documents remain on the task branch.
Existing user artifacts `pass1.txt`–`pass6.txt`, `repiu.exe`, and `repiu150.exe` are excluded from
the commit.
