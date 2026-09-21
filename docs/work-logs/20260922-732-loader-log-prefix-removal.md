# Task 732 작업 로그 — 로더 로그의 `Win32` 접두어 제거

설계: [20260922-732](../design/20260922-732-loader-log-prefix-removal.md)
작업 지시: [20260922-732](../work-orders/20260922-732-loader-log-prefix-removal.md)

## 결과

로더 문자열 리터럴 시작의 `"Win32 ` 646개를 제거했습니다. 줄은 이제
`[loader] minimal execution timed out: true`처럼 읽힙니다. 변경은 **리터럴 시작의 접두어 제거만**입니다.
HEAD 파일에 같은 치환을 적용한 결과와 작업 파일을 `cmp`로 비교해 바이트 단위로 같음을 확인했습니다.

소비자는 로더의 실제 리터럴 646개로 만든 판정 목록으로 230곳을 치환했습니다.

| 파일 | 치환 |
|---|---:|
| `scripts/test_all.ps1` | 93 (+ 중첩 괄호 1곳 수동) |
| 그 밖의 `scripts/task*.ps1`, `test_openwatcom_samples.ps1` | 81 |
| `docs/guides/` 7개 | 61 |
| `ARCHITECTURE.md` | 2 (+ Task 731 문장 수동) |

보존한 125곳은 모두 검토했습니다. CI job 이름(`Win32 Debug build`, `Win32 Release build`), 빌드 단계 이름
(`Build Win32 x86 host`), 스크립트 자체 메시지(`Win32 supervisor was not found`,
`Win32 loader was not found`), 본문의 플랫폼 서술(`Win32 host pointer size`,
`Win32 loader executable target`) 등으로, 모두 실제 플랫폼을 말합니다. 판정기가 처리하지 못한 것은
`test_all.ps1` 282행의 중첩 괄호 하나였고 수동으로 고쳤습니다.

`tests/history/`, 작업 로그, 설계, 작업 지시, 분석 문서의 과거 인용은 당시 출력의 기록이므로 바꾸지
않았습니다.

## 충돌 검사

기존 로그 3개(Win32 `task458-final`, Linux `task731-linux-run`, `task729-crash/on-1`)에서 패턴 746개를
비교했습니다. 매칭 수가 달라진 것은 4개였고 로그 3개에서 같았습니다. 모두 소비자에서 온 패턴이
아니었습니다. 세 개(`Glide first triangle vertex`, `execution backend:`, `reserved size:`)는 검사기가
리터럴을 placeholder에서 자른 결과이고 어떤 스크립트나 가이드도 쓰지 않습니다. 나머지 `allocator`도
같은 절단의 산물이며, `test_all.ps1`이 실제로 쓰는 `allocator probe observation count` 등은 통과했습니다.

## `test_all.ps1` 단정 비교 — 치환은 동등하고, 스위트는 이미 낡아 있었다

`test_all.ps1`은 이 머신에 없는 `build\win32_x86_debug`를 가리키므로 그대로 실행하면 새 트리 전체를
빌드합니다. 대신 binary 경로만 `build\Debug`로 바꾸고 빌드 단계를 뺀 사본을 만들어, pumpit1 단정
99개를 이름 변경 전후에 각각 적용했습니다.

| | 불일치 단정 | 프로세스 crash/hang |
|---|---:|---:|
| 이름 변경 전 binary + 이전 `test_all.ps1` (정상 종료한 실행) | 17 | 6회 중 4회 |
| 이름 변경 후 binary + 새 `test_all.ps1` (정상 종료한 실행) | 17 | 4회 중 1회 |

두 17개는 `Win32 ` 접두어만 빼면 **같은 패턴**입니다. 따라서 소비자 치환은 동등하고, 17개 실패와 간헐
crash(0xC0000005)·hang은 이름 변경 이전부터 있던 것입니다. 이름 변경 후 다른 한 실행은 clean teardown
경로(`hijacked thread for clean teardown`)를 타서 13개가 불일치했습니다. 실행마다 도달하는 결말이 달라
불일치 수가 달라집니다.

17개는 모두 내용이 달라진 것입니다.

* 결말 문구: `minimal execution attempt timed out` → 현재 `timeout reached; guest thread was not in
  recoverable code` 등(Task 507 이후).
* DOS 흔적: `DOS environment access observed: true` → 현재 `false`, 마지막 open `intro.ani|stage.cfg`
  → 현재 `spr.res`, path trace #1·#2 내용.
* 예외로 멈추는 결말 집합과 `Current execution blocker ...`.

`test_all.ps1`의 마지막 수정은 2026-08-15, `tests/history/`의 마지막 기록은 2026-08-06입니다. 그
뒤로 게스트가 더 멀리 실행되면서 단정이 낡았고, 잘못된 빌드 경로 때문에 아무도 돌리지 않아 드러나지
않았던 것으로 보입니다. **이 작업은 단정을 고치지 않았습니다.** 회귀 기준을 새로 정하는 일은 별도
결정입니다.

## 검증

- Win32 x86 Debug 전체 빌드 성공, core probe 29/29. pumpit1 출력의 `[loader]` 줄 564개 중 `Win32`
  접두어 0.
- WSL Linux x64 Debug 빌드 성공, core probe 31/31. `pumpit2a` 15초 실행의 `[loader]` 줄 817개 중
  `Win32` 접두어 0, fault 0.
- 판정기 dry run의 치환·보존 목록 전체 검토, 충돌 검사, `test_all.ps1` 단정의 전후 비교(위).
- 참고: A/B 도중 `Copy-Item`이 원본의 수정 시각을 보존해 MSBuild가 재컴파일을 건너뛴 일이 있었습니다.
  timestamp를 갱신해 재빌드했고, 위 수치는 재빌드한 binary의 것입니다.

---

# English

# Task 732 work log — removing the `Win32` prefix from loader logs

Design: [20260922-732](../design/20260922-732-loader-log-prefix-removal.md)
Work order: [20260922-732](../work-orders/20260922-732-loader-log-prefix-removal.md)

## Result

The 646 `"Win32 ` string-literal starts in the loader were removed, so lines read
`[loader] minimal execution timed out: true`. The change is **only the literal-start prefix
removal**: applying the same substitution to the HEAD file and comparing with `cmp` shows the working
file is byte-identical.

Consumers were rewritten at 230 places from a decision list built from the loader's 646 actual
literals: 93 in `scripts/test_all.ps1` (plus one nested-parenthesis case by hand), 81 in the other
`scripts/task*.ps1` and `test_openwatcom_samples.ps1`, 61 across seven `docs/guides/`, and 2 in
`ARCHITECTURE.md` (plus Task 731's sentence by hand).

All 125 kept occurrences were reviewed: CI job names (`Win32 Debug build`, `Win32 Release build`), a
build step name (`Build Win32 x86 host`), scripts' own messages (`Win32 supervisor was not found`,
`Win32 loader was not found`), and prose about the platform (`Win32 host pointer size`,
`Win32 loader executable target`) -- all naming the actual platform. The one case the classifier could
not handle was the nested parentheses on `test_all.ps1` line 282, fixed by hand.

Past quotations in `tests/history/`, work logs, designs, work orders and analysis documents record
the output of their time and were left unchanged.

## Collision check

746 patterns were compared on three existing logs (Win32 `task458-final`, Linux `task731-linux-run`
and `task729-crash/on-1`). Four changed their match count, the same four in all three logs, and none
comes from a consumer. Three (`Glide first triangle vertex`, `execution backend:`, `reserved size:`)
are artifacts of the checker cutting literals at their placeholders and are used by no script or
guide. `allocator` is the same kind of artifact; the patterns `test_all.ps1` actually uses, such as
`allocator probe observation count`, passed.

## `test_all.ps1` assertions compared — the rewrite is equivalent, and the suite was already stale

`test_all.ps1` points at `build\win32_x86_debug`, which does not exist on this machine, so running it
as-is would build a whole new tree. Instead a copy with only the binary path changed to `build\Debug`
and the build steps removed applied the 99 pumpit1 assertions before and after the rename.

| | Unmatched assertions | Process crash/hang |
|---|---:|---:|
| Pre-rename binary + old `test_all.ps1` (a clean run) | 17 | 4 of 6 runs |
| Post-rename binary + new `test_all.ps1` (a clean run) | 17 | 1 of 4 runs |

The two sets of 17 are **the same patterns** apart from the `Win32 ` prefix, so the consumer rewrite
is equivalent, and the 17 failures and the intermittent crash (0xC0000005) and hang predate the
rename. One other post-rename run took the clean-teardown path (`hijacked thread for clean
teardown`) and had 13 unmatched; the ending a run reaches varies, and so does the count.

All 17 are content changes: the ending wording (`minimal execution attempt timed out` is now `timeout
reached; guest thread was not in recoverable code` and similar, since Task 507); DOS traces
(`DOS environment access observed: true` is now `false`, the last open is `spr.res` rather than
`intro.ani|stage.cfg`, and path trace #1 and #2 differ); and the exception-ending set with its
`Current execution blocker ...` line.

`test_all.ps1` was last modified on 2026-08-15 and `tests/history/` last recorded on 2026-08-06. The
guest has since run further, the assertions went stale, and the wrong build path appears to have kept
anyone from running the suite to notice. **This task did not fix the assertions**; setting a new
regression baseline is a separate decision.

## Verification

- Full Win32 x86 Debug build succeeded; core probe 29/29. Of 564 `[loader]` lines in pumpit1 output,
  zero carry the `Win32` prefix.
- WSL Linux x64 Debug build succeeded; core probe 31/31. Of 817 `[loader]` lines in a 15-second
  `pumpit2a` run, zero carry the prefix, with zero faults.
- The classifier's full replaced and kept lists were reviewed, plus the collision check and the
  before-and-after assertion comparison above.
- Note: during the A/B, `Copy-Item` preserved the source file's modification time and MSBuild skipped
  the recompile. The timestamp was refreshed and the numbers above come from the rebuilt binary.
