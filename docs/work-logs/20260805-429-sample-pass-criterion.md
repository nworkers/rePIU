# Task 429 작업 로그 — 샘플 하네스 통과 기준에 완주 요구 추가

지시: [20260805-429](../work-orders/20260805-429-sample-pass-criterion.md) ·
관련 정정: [Task 428 작업 로그](20260805-428-openwatcom-baseline-refresh.md) §4

## 1. 한 일

`scripts/test_openwatcom_samples.ps1`의 통과 판정에 **완주 요구**를 추가했습니다.

```powershell
# 이전 — timeout도 만족합니다
$runPassed = $run.ExitCode -eq 0 -and
             $run.Output -match "Win32 minimal execution exception caught: false"

# 이후
$runPassed =
    $run.ExitCode -eq 0 -and
    $run.Output -match "Win32 minimal execution exception caught: false" -and
    $run.Output -match "Win32 minimal execution returned: true" -and
    $run.Output -notmatch "minimal execution attempt timed out"
```

함께 `$script:RunCriterionId`("exit0+no-exception+returned+no-timeout")를 도입해
summary와 baseline에 `RunCriterion` 필드로 기록하고, `-CompareBaseline`이 baseline의
값과 다르면 경고하도록 했습니다.

## 2. 결함의 성격

이것은 하네스가 **실행 결과를 잘못 읽던** 문제이지, 로더나 backend의 회귀가
아닙니다. 게스트가 멈춘 채 로더의 기본 1,000 ms timeout이 끝나면 예외는 잡히지 않고
프로세스는 0으로 종료합니다. 옛 판정식은 그 둘만 봤으므로 timeout이 통과가 됐습니다.

`dynamic`에서만 통과하던 39개 중 8개 표본에서 **절반이 위양성**이었습니다.

| 샘플 | `returned` | 로더 메시지 |
|---|---|---|
| `_searche.c`, `abrt_hnd.c`, `asctim_s.c`, `asctime.c` | true | original entry returned to host trampoline |
| `b_equip.c`, `b_memsiz.c`, `b_print.c`, `b_serial.c` | **false** | **minimal execution attempt timed out** |

`b_*` 계열은 `INT 11h`(BIOS 장비 목록)를 씁니다. 이 인터럽트는 **양쪽 backend 모두
미구현**입니다. legacy는 이를 잡아 `unsupported DOS interrupt 0x11`로 이름 붙여
실패로 보고하고, `dynamic`에서는 예외로 표면화되지 않아 timeout을 소진합니다.

## 3. 확인됨 — legacy의 낮은 통과율은 호환성 열세가 아닙니다

Task 428 §4가 39개 차이를 정확성 우위로 읽은 것은 틀렸습니다. 실제 원인은 둘로
나뉩니다.

1. **절반은 위양성**입니다(§2). 같은 인터럽트가 양쪽 모두 미구현이며, legacy가 더
   정직하게 실패를 보고합니다.
2. **나머지 절반은 HLE 표면 크기 차이**입니다. 샘플은 `dos4gw_console_sample`
   프로파일이라 legacy에서 `AttemptWin32GuestStackHleExecution`을 타고, 그 경로의
   `HandleDosHleInstruction`은 `INT 21h`·`2Fh`·`31h`·`33h`·`16h` 다섯 개만
   처리합니다. `dynamic`은 AOT 경로로 전체 HLE dispatcher table을 씁니다.

**어느 쪽도 실행 방식(single-step 대 번역)의 충실도 차이가 아닙니다.** legacy 진입점
배선의 문제이며, 필요하면 별도 작업으로 좁은 HLE 표면을 넓힐 수 있습니다.

## 4. baseline 불일치 — 의도적으로 남긴 상태

사용자 요청에 따라 **스위트를 재실행하지 않았습니다.** 따라서 현재 baseline의
`RunPassed` 535는 **옛 기준으로 측정된 값**입니다.

**강화된 기준의 첫 실행은 회귀를 보고할 수 있습니다.** timeout으로만 통과하던
샘플이 이제 실패로 집계되기 때문이며, 이는 코드 회귀가 아니라 측정 정정입니다.
`-CompareBaseline`이 다음 경고를 냅니다.

```
Baseline pass criterion differs from the current one.
baseline=(none - predates Task 429) current=exit0+no-exception+returned+no-timeout.
Reported regressions may be measurement corrections, not code regressions.
Re-record the baseline with -UpdateBaseline once the difference has been reviewed.
```

**legacy 통과 535개 중 몇 개가 timeout이었는지는 미확인입니다.** baseline에 `Detail`
필드가 저장되지 않아 재실행 없이는 셀 수 없습니다. 다음 실행에서 드러납니다.

## 5. 검증

스위트를 돌리지 않았으므로 두 가지로 확인했습니다.

### 5.1 판정식 replay — 실제 로더 출력 대상

| 샘플 | backend | 옛 판정 | 새 판정 | 기대 | 결과 |
|---|---|---|---|---|---|
| `_exit.c` | legacy | pass | pass | pass | OK |
| `_atouni.c` | legacy | pass | pass | pass | OK |
| `b_equip.c` | dynamic | **pass** | **fail** | fail | OK |
| `b_print.c` | dynamic | **pass** | **fail** | fail | OK |
| `asctime.c` | dynamic | pass | pass | pass | OK |
| `b_equip.c` | legacy | fail | fail | fail | OK |

**6건 전부 일치.** 진짜 통과 3건이 유지되어 위음성이 없고, timeout 2건만
`pass → fail`로 뒤집혔습니다.

### 5.2 구문

PowerShell 파서로 `ParseFile` 검사 — 오류 0건.

## 6. 남은 것

* 다음 스위트 실행에서 새 기준의 실제 수치를 확인하고, 검토 후 `-UpdateBaseline`으로
  baseline을 재기록합니다.
* 출력 정확성 검증은 이번 범위 밖입니다. 샘플별 기대 출력이 없어 별도 설계가
  필요합니다.
* legacy 콘솔 샘플 경로의 좁은 HLE 표면(§3-2)은 별도 작업 후보입니다.

---

# Task 429 Work Log — requiring completion in the sample harness pass criterion

Work order: [20260805-429](../work-orders/20260805-429-sample-pass-criterion.md). Related
correction: [Task 428 work log](20260805-428-openwatcom-baseline-refresh.md) §4.

## 1-2. What was done, and the nature of the defect

The run-pass criterion now also requires `Win32 minimal execution returned: true` and the
absence of `minimal execution attempt timed out`, alongside the existing exit code and
"exception caught: false". A `$script:RunCriterionId` of
`exit0+no-exception+returned+no-timeout` is recorded into the summary and baseline as
`RunCriterion`, and `-CompareBaseline` warns when a baseline's value differs or is missing.

This was the harness **misreading results**, not a loader or backend regression: a stalled guest
lets the default 1,000 ms timeout expire with nothing caught and a zero exit, which the old
two-condition test accepted. Four of eight sampled dynamic-only passes were such timeouts
(`b_equip.c`, `b_memsiz.c`, `b_print.c`, `b_serial.c`), against four genuine completions.

## 3. Confirmed — legacy's lower pass rate is not weaker compatibility

Task 428 §4 read the 39-sample gap as a correctness advantage; that was wrong. Half were false
passes on `INT 11h` (BIOS equipment list), which **neither backend implements** — legacy simply
catches and names the failure while `dynamic` stalls into a timeout. The other half come from
HLE surface size: samples use the `dos4gw_console_sample` profile, so legacy takes
`AttemptWin32GuestStackHleExecution`, whose `HandleDosHleInstruction` covers only `INT 21h`,
`2Fh`, `31h`, `33h`, and `16h`, while `dynamic` takes the AOT path with the full dispatcher
table. **Neither cause is a difference in execution fidelity** between single-stepping and
translation; it is how the legacy entry point is wired.

## 4. Baseline mismatch, left deliberately

The suite was **not re-run** by request, so the baseline's `RunPassed` of 535 was measured under
the old rule. **The first run under the new rule may report regressions** as samples that only
ever passed by timing out are now scored as failures — measurement corrections, not code
regressions — and `-CompareBaseline` emits a warning naming both criteria and recommending a
re-record after review. **How many of the 535 legacy passes were themselves timeouts is
unknown**: the baseline does not store the `Detail` field, so it cannot be counted without a
re-run. The next run will show it.

## 5. Verification

Replaying both predicates against real captured loader output across six cases — two genuine
legacy passes, two `dynamic` timeouts, one genuine `dynamic` completion, one genuine legacy
failure — matched expectations in all six: the three genuine passes held (no false negatives)
and only the two timeouts flipped from pass to fail. The modified script parses cleanly with
zero PowerShell parser errors.

## 6. Remaining

Confirm the new figures on the next suite run and re-record the baseline with `-UpdateBaseline`
after review. Output-correctness checking remains out of scope pending a design for expected
output. The narrow HLE surface on the legacy console-sample path is a candidate for its own
task.
