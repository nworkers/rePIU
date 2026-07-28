# 20260728-342 설계: 반복 쓰기에만 quarantine / Design: Quarantine only on repeated writes

## 한국어

### 1. 왜 필요한가

Task 341이 확정한 사실입니다. 게임은 자기 `out` 명령을 1~2바이트 덮어쓰는 세 지점을
가지고 있고, 그 **한 번의 쓰기가 4KB 페이지를 영구히 번역 대상에서 제외**합니다.
그중 `0x030F5000`은 이 실행에서 가장 뜨거운 코드 페이지입니다.

결과는 Task 340이 측정했습니다. post-HLE 복귀 시도의 **80.24%가 quarantine으로
거절**되고, 그 페이지의 실행은 전부 TF walk가 됩니다. Task 337의 5~8개 구간(step의
54%)과 33+ 꼬리(27%)가 여기서 나옵니다.

**즉 60초에 3번 일어나는 사건이 실행 시간의 큰 부분을 결정합니다.**

### 2. quarantine이 지키는 것

현재 정책은 `NoteSuccessfulAotGuestWrite`에서 이렇습니다.

```
쓰기 주체 페이지 == 쓰인 페이지  ->  retire + quarantine
그 외                          ->  retire만
```

두 경로 모두 **retire**를 합니다. retire는 그 페이지의 기존 번역을 무효화하므로,
**캐시가 옛 바이트를 실행하는 일은 retire만으로 이미 막힙니다.** 이후 그 주소에
다시 도달하면 새 바이트로 재번역됩니다.

**quarantine이 추가로 막는 것은 정확성이 아니라 churn입니다.** 자기 페이지에 계속
쓰는 코드는 번역→무효화→재번역을 반복하므로, 아예 번역 대상에서 빼는 것이 그
방어입니다.

**그런데 이 세 지점은 각각 한 번만 씁니다.** 방어할 churn이 없는데 영구 배제만
남았습니다.

### 3. 선택한 정책

**같은 페이지에 대한 쓰기 횟수를 세고, 임계값을 넘을 때만 quarantine합니다.**

```
same-page 쓰기 1회차 ~ (N-1)회차  ->  retire만 (재번역 허용)
same-page 쓰기 N회차 이상         ->  retire + quarantine (기존 동작)
```

`N = 4`로 시작합니다. 관측된 세 지점은 1회이므로 재번역되고, 진짜 자기수정 코드는
네 번째 쓰기에서 기존 방어로 돌아갑니다.

**정확성은 바뀌지 않습니다.** 위 2절대로 옛 바이트 실행은 retire가 막고, quarantine은
churn 방어일 뿐입니다. 재번역은 항상 그 시점의 guest 바이트를 읽습니다.

**패치된 명령이 HLE 경계(`out`)라는 점도 이 정책이 자연스럽게 처리합니다.** 재번역은
새 바이트를 다시 분류하므로, 패치 후 `out`이 아니게 되면 경계도 아니게 됩니다.
바이트를 보존한 채 범위만 제외하는 방식(대안 B)은 이 재분류를 스스로 해야 합니다.

### 4. 기각한 대안

* **대안 B — 패치 범위만 `AotExcludedGuestRange`로 제외하고 재번역.** 기계장치는
  있지만, 제외된 주소는 경계 `INT3`로 남으므로 그 명령은 **영구히 예외 경로**가
  됩니다. 패치의 목적이 그 `out`을 없애는 것이라면 오히려 손해입니다.
* **대안 C — 바이트 검증 후 quarantine 페이지 복귀 허용.** 바이트가 변했으므로
  검증은 실패합니다. 이 사례에 맞지 않습니다.

### 5. 사전 등록 gate

| gate | 조건 | 의미 |
|---|---|---|
| **G1** | quarantine 이벤트 수 0 | 세 지점이 더는 페이지를 배제하지 않는다 |
| **G2** | 복귀 funnel의 success 비중 >= 50% | 막힌 원인이 실제로 제거됐다 |
| G3 | single-step 예외 수 감소 | walk가 줄었다 |
| **G4** | 프레임 중앙값 증가(3회) | 처리량으로 이어졌다 |
| G5 | G1은 성립하나 G2 기각 | 다른 거절 사유가 드러난 것이므로 재측정 |

### 6. 동등성 계약 (Task 338 이후)

malformed 0, fatal 0, Glide 공백 0, 60초 정상 timeout에 더해 **`grBufferSwap` 횟수,
Glide gate 진입 횟수, LINEXE get-proc 개수**가 baseline 범위 안에 있어야 합니다.
`progress`만으로 판단하지 않습니다.

### 7. 되돌릴 수단

`REPIU_AOT_QUARANTINE_FIRST_WRITE=1`이면 기존 동작(첫 same-page 쓰기에 즉시
quarantine)으로 돌아갑니다. A/B를 한 바이너리에서 수행하기 위한 것입니다.

---

## English

### 1. Why

Task 341 established that the game overwrites one or two bytes of its own `out` instruction at
three sites, and that each single write permanently excludes a 4KB page from translation — one of
them, `0x030F5000`, being the hottest code page in the run. Task 340 measured the consequence:
80.24% of post-HLE returns are rejected as quarantined and everything on those pages executes as a
TF walk, which is Task 337's five-to-eight-step mode and long tail. Three events in 60 seconds
determine a large share of execution time.

### 2. What quarantine actually protects

Today a write whose source page equals the written page retires and quarantines the page, while any
other write only retires it. Both paths retire, and retiring already invalidates that page's
translations, so the cache can never execute stale bytes on either path; reaching the address again
re-translates from the current bytes. What quarantine adds is not correctness but churn control,
for code that keeps writing its own page and would otherwise translate, invalidate, and translate
again. These three sites write once each, so there is no churn to prevent — only permanent
exclusion left behind.

### 3. The chosen policy

Count writes per page and quarantine only past a threshold: the first `N-1` same-page writes retire
only, and the `N`-th and later retire and quarantine as before, with `N` starting at 4. The three
observed sites write once, so their pages are re-translated, while genuinely self-modifying code
falls back to the existing defense on its fourth write. Correctness is unchanged for the reason
above, and re-translation always reads the bytes as they are at that moment. This also handles the
patched instruction being an HLE boundary: re-translation reclassifies the new bytes, so an `out`
that has been patched away stops being a boundary.

### 4. Rejected alternatives

Excluding only the patched range with `AotExcludedGuestRange` keeps the machinery but leaves that
address as a boundary `INT3` forever, which is the opposite of what a patch removing the `out` is
for. Allowing return into a quarantined page after verifying bytes are unchanged does not apply,
since the bytes did change.

### 5. Pre-registered gates

G1 holds if quarantine events reach zero; G2 if the return funnel's success share reaches 50%; G3
if single-step exceptions fall; G4 if the median frame count over three runs rises; and G5 covers
G1 holding while G2 is rejected, which would mean another rejection reason has surfaced and needs
measuring.

### 6. Equivalence contract

Beyond zero malformed dispatch, no fatal halt, no Glide gap, and a normal 60-second timeout, the
buffer-swap count, the Glide gate entry count, and the resolved LINEXE proc count must stay in the
baseline's range, as Task 338 required. Progress alone does not decide.

### 7. Reversal

`REPIU_AOT_QUARANTINE_FIRST_WRITE=1` restores quarantining on the first same-page write so the A/B
runs from one binary.
