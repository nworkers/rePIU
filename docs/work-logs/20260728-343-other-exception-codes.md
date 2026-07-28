# 20260728-343 작업 로그: "other" 예외의 정체 / Work log: What "other" exceptions are

## 한국어

### 결론 요약

**Task 342가 만든 1,931건의 "other" 예외는 `0xC0000096`
(`STATUS_PRIVILEGED_INSTRUCTION`) 하나뿐입니다.** 결함이 아니라 **의도한 변화의
직접적 결과**입니다.

quarantine이 풀린 페이지가 이제 번역되므로, 그 안의 특권 명령(`out` 등)이 **캐시에서
직접 실행되다가 #GP로 트랩**합니다. 이전에는 같은 명령을 TF walk로 걸어가며
single-step 경로에서 처리했습니다. 즉 **처리 경로가 single-step에서 특권 명령 트랩으로
옮겨간 것**이고, 두 경로 모두 기존 HLE(`HandlePrivilegedTrapInstruction`)가 받습니다.

### 측정 값 (Task 342 정책, Release 60초 1회)

| 예외 종류 | 횟수 | 비중 |
|---|---:|---:|
| TF single-step | 230,272 | 51.54% |
| `INT3` | 203,244 | 45.49% |
| access violation | 11,304 | 2.53% |
| **`0xC0000096` 특권 명령** | **1,927** | **0.43%** |
| 합계 | 446,747 | 100% |

같은 실행의 프레임은 3,216이고 quarantine 이벤트는 1건, 유예 6건입니다.

**확인됨:** 코드 종류는 **하나뿐**입니다(overflow 0). 미지의 예외가 섞여 들어온 것이
아닙니다.

**확인됨:** 예외 구성이 바뀌었습니다. Task 337의 baseline에서는 single-step 79.24% /
`INT3` 19.59%였는데, 지금은 51.54% / 45.49%입니다. **walk가 줄고 경계 트랩이
늘었습니다** — 번역 커버리지가 넓어졌다는 뜻입니다.

### 확인됨 / Confirmed

* "other"는 전부 `STATUS_PRIVILEGED_INSTRUCTION`이며 0.43%입니다.
* 기존 특권 명령 HLE가 그대로 처리하며 동등성 축은 모두 통과합니다.

### 미확정 / Unresolved

* 특권 명령을 캐시에서 트랩시키는 것과 HLE 경계 `INT3`로 미리 표시하는 것 중
  어느 쪽이 싼지는 재지 않았습니다. 1,927건뿐이라 우선순위는 낮습니다.

---

## English

The 1,931 "other" exceptions Task 342 introduced are a single code, `0xC0000096`
(`STATUS_PRIVILEGED_INSTRUCTION`), at 1,927 occurrences and 0.43% of all exceptions, with no
overflow, so nothing unknown crept in. They are the direct consequence of the intended change:
pages that are no longer quarantined are now translated, so privileged instructions such as `out`
execute from the cache and trap with #GP instead of being walked under TF and handled on the
single-step path. Both routes end in the same `HandlePrivilegedTrapInstruction` HLE.

The exception mix has shifted with translation coverage: Task 337's baseline was 79.24%
single-step against 19.59% `INT3`, and this run is 51.54% against 45.49%, with 3,216 frames. Left
unresolved: whether trapping privileged instructions from the cache is cheaper than marking them as
HLE boundaries up front, which at 1,927 occurrences is low priority.
