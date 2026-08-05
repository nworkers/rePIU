# Task 429 작업 지시 — 샘플 하네스 통과 기준에 완주 요구 추가

발견 경위: [Task 428 작업 로그](../work-logs/20260805-428-openwatcom-baseline-refresh.md) §4
이후, "왜 legacy가 dynamic보다 성공률이 낮은가"를 추적하다 드러난 계측 결함

## 1. 결함

`scripts/test_openwatcom_samples.ps1`의 통과 판정이 두 조건뿐이었습니다.

```powershell
$runPassed = $run.ExitCode -eq 0 -and
             $run.Output -match "Win32 minimal execution exception caught: false"
```

**timeout이 이 두 조건을 모두 만족합니다.** 게스트가 멈춘 채 로더의 기본 1,000 ms
실행 timeout이 끝나면 예외는 잡히지 않고 로더는 0으로 종료합니다. 완주 여부도
출력 정확성도 검사하지 않으므로 통과로 집계됩니다.

`dynamic`에서만 통과하던 39개 중 8개를 표본 검사한 결과입니다.

| 샘플 | `returned` | 로더 메시지 | 판정 |
|---|---|---|---|
| `_searche.c`, `abrt_hnd.c`, `asctim_s.c`, `asctime.c` | true | original entry returned to host trampoline | 진짜 통과 |
| `b_equip.c`, `b_memsiz.c`, `b_print.c`, `b_serial.c` | **false** | **minimal execution attempt timed out** | **위양성** |

절반이 위양성이었습니다.

## 2. 변경

### 2.1 통과 기준에 완주 요구 추가

```powershell
$runPassed =
    $run.ExitCode -eq 0 -and
    $run.Output -match "Win32 minimal execution exception caught: false" -and
    $run.Output -match "Win32 minimal execution returned: true" -and
    $run.Output -notmatch "minimal execution attempt timed out"
```

`returned: true`가 완주 신호입니다. timeout은 `returned: false`로 나오므로 이
조건만으로도 걸리지만, 방어하려는 실패 형태를 이름으로 남기기 위해 timeout 문구
검사도 함께 둡니다.

**출력 정확성 검증은 이번 범위가 아닙니다.** 샘플마다 기대 출력이 없으므로 별도
설계가 필요합니다.

### 2.2 기준 식별자 기록

`$script:RunCriterionId`를 summary와 baseline에 `RunCriterion` 필드로 남깁니다.
기준이 바뀌면 이 값을 바꿉니다.

### 2.3 기준 불일치 경고

`-CompareBaseline`이 baseline의 `RunCriterion`을 현재 값과 비교해 다르면
`Write-Warning`을 냅니다. 필드가 없는 baseline은 Task 429 이전 것입니다.

**이 경고가 이번 작업의 핵심 안전장치입니다.** 사용자 요청에 따라 baseline을
재측정하지 않으므로, 현재 baseline(535)은 **옛 기준으로 측정된 값**입니다. 강화된
기준의 첫 실행은 timeout으로만 통과하던 샘플을 회귀로 보고할 텐데, 그것은 코드
회귀가 아니라 측정 정정입니다. 경고가 없으면 반드시 오독됩니다.

## 3. 하지 않을 것

* **스위트 재실행과 baseline 재측정을 하지 않습니다.** 다음 실행부터 새 기준이
  적용됩니다.
* `RunPassed` 535가 옛 기준 값이라는 사실은 문서로만 남깁니다.

## 4. 검증

스위트를 돌리지 않으므로 다음 두 가지로 확인합니다.

| # | 확인 | 통과 조건 |
|---:|---|---|
| 1 | 기준 판정식 replay | 실제 로더 출력에 옛/새 판정식을 각각 적용해, 진짜 통과는 유지되고 timeout만 뒤집힘 |
| 2 | 스크립트 구문 | PowerShell 파서 오류 0건 |

1번 케이스는 최소 여섯 가지를 포함합니다 — legacy 정상 통과 2건(위음성 없음 확인),
`dynamic` timeout 2건(위양성 제거 확인), `dynamic` 진짜 완주 1건, legacy 진짜 실패 1건.

## 5. 완료 기준

1. §4의 두 확인이 통과했습니다.
2. Task 428 작업 로그 §4의 잘못된 서술을 정정했습니다.
3. 작업 로그에 결함, 근거, baseline 불일치 처리 방침을 남겼습니다.

---

# Task 429 Work Order — require completion in the sample harness pass criterion

Surfaced while tracing why legacy showed a lower pass rate than dynamic, following
[the Task 428 work log](../work-logs/20260805-428-openwatcom-baseline-refresh.md) §4.

## 1. The defect

The harness scored a run as passing on exit code plus "exception caught: false". **A timeout
satisfies both**: the guest stalls, the loader's default 1,000 ms execution timeout expires,
nothing is caught, and the process exits 0. Neither completion nor output correctness was
checked. Of eight sampled from the 39 dynamic-only passes, four never completed —
`b_equip.c`, `b_memsiz.c`, `b_print.c`, and `b_serial.c` report `returned: false` with
`minimal execution attempt timed out`, against `returned: true` for the four genuine ones.

## 2. The change

Add `returned: true` and the absence of the timeout message to the criterion; `returned: true`
alone would suffice, but naming the timeout keeps the guarded failure mode explicit. **Output
correctness is out of scope** — the samples carry no expected output, which needs its own
design.

Record `$script:RunCriterionId` into the summary and baseline as `RunCriterion`, and have
`-CompareBaseline` warn when a baseline's value differs or is absent. **That warning is the
safety mechanism of this task**: the baseline is deliberately not re-recorded, so the 535 was
measured under the old rule, and the first run under the new rule will report samples that only
ever passed as timeouts. Those are measurement corrections, not code regressions, and without
the warning they will be misread.

## 3. Out of scope

No suite re-run and no baseline re-record — the new criterion applies from the next run onward.
That 535 reflects the old rule is recorded in documentation only.

## 4-5. Verification and completion

Without running the suite, verification is a replay of both predicates against real captured
loader output — at least six cases covering two genuine legacy passes (no false negatives), two
`dynamic` timeouts (false positives removed), one genuine `dynamic` completion, and one genuine
legacy failure — plus a clean PowerShell parse of the modified script. Done when both checks
pass, the incorrect claim in the Task 428 work log is corrected, and the work log records the
defect, the evidence, and the baseline-mismatch policy.
