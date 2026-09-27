# Task 751: 롬셋 전수 조사와 게스트 `cli` 보존 작업 지시

설계: [20260928-751](../design/20260928-751-guest-cli-hold-and-romset-survey.md)

## 한국어

1. `scripts/survey_romsets.sh`와 `scripts/input_scripts/survey_generic.txt`로 22개 프로필을 Linux x64와
   Win32에서 60초씩 조사한다.
2. 죽는 롬셋의 원인을 찾는다(죽는 명령, 그 호출 경로, `scripts/cat702_table_check.py`로 보안 검사 표의
   오프라인 대조).
3. `PicTimerInService`에 cli hold(`cli`/`sti`/주입 프레임의 `iret`, 100 ms 밸브, 계수기)를 넣고
   `InjectPendingInterrupts`가 따르게 한다. `REPIU_GUEST_CLI_HOLD=0`으로 끈다.
4. Linux x64의 privileged HLE 체인과 planner-HLE 주입에 `CanEnterTimerInterruptHandler` 확인을 넣고, 주입
   뒤 cache로 진입하게 한다.
5. 주입 프레임 추적으로 핸들러 반환을 가리고, 중첩 깊이·`sti` 연쇄·반환 직후 연쇄·메인 코드의 차례
   규칙을 넣는다.
6. `AAM`/`AAD` HLE를 넣는다.
7. `REPIU_PIU10_CAT702_TRACE`, 최종 보고 줄, probe 케이스를 넣는다.
8. 반복 시작·재조사(Linux x64, Win32)·core probe로 검증하고 README·analysis·작업 로그를 갱신한 뒤
   커밋한다.

## English

1. Survey the 22 profiles for 60 s each on Linux x64 and Win32 with `scripts/survey_romsets.sh` and
   `scripts/input_scripts/survey_generic.txt`.
2. Find why the failing ROM sets die (the instruction, its call path, an offline check of the security
   tables with `scripts/cat702_table_check.py`).
3. Add the cli hold to `PicTimerInService` (`cli`/`sti`/the injected frame's `iret`, the 100 ms valve,
   counters) and make `InjectPendingInterrupts` obey it; `REPIU_GUEST_CLI_HOLD=0` turns it off.
4. Put the `CanEnterTimerInterruptHandler` check in Linux x64's privileged HLE chain and planner-HLE
   injection, and enter through the cache after an injection.
5. Tell a handler's return by tracking the injected frames, and add the rules for nesting depth, the
   `sti` chain, the chain after a return and the interrupted code's turn.
6. Add the `AAM`/`AAD` HLE.
7. Add `REPIU_PIU10_CAT702_TRACE`, the final report lines and the probe cases.
8. Verify with repeated starts, a second survey (Linux x64, Win32) and the core probes, update README,
   the analysis and the work log, and commit.
