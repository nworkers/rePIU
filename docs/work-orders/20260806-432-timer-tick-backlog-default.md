# Task 432 작업 지시 — timer tick backlog 기본값 전환

설계: [20260806-432](../design/20260806-432-timer-tick-backlog-default.md)

## 1. 범위

`REPIU_TIMER_TICK_BACKLOG`의 기본값을 **켬**으로 바꿉니다. 새 기구는 만들지 않습니다 —
Task 366이 만들어 둔 backlog를 기본 경로로 올릴 뿐입니다.

## 2. 변경할 파일

| 파일 | 내용 |
|---|---|
| `timer_tick_delivery.h` | 기본값 주석 갱신, 366 측정에 **SUPERSEDED** 표기, 시그니처를 `const char*`로 |
| `timer_tick_delivery.cpp` | `ResolveTimerTickBacklogEnabled` 기본 true, 명시적 off만 끔 |
| `timer_tick_delivery_probe.cpp` | 정책 단정을 새 기본값에 맞춰 갱신(미설정·미인식 값 → 켬) |
| `docs/analysis/current-execution-frontier.md` | 항목 1‴b 해소 |
| `docs/work-logs/20260730-366-*` · `docs/design/20260730-366-*` | 결론이 현재 빌드에 적용되지 않음을 머리말에 명시 |
| `docs/guides/cd-audio-position-census.md` | 기본값 변경 반영 |

**건드리지 않을 것:** backlog 상한(64), 안전점 개수, 주입 정책, rendezvous 스핀.

## 3. 구현 규칙

* **끄는 경로를 남깁니다.** `REPIU_TIMER_TICK_BACKLOG=0`은 회귀 대조군이며, 366이 이
  스위치를 남겨 둔 덕분에 이번 진단이 가능했습니다.
* **미인식 값은 켬으로 처리합니다.** 변수 오타가 조용히 틱 손실 경로로 되돌리면 안 됩니다.
* 366의 측정치는 **지우지 않고** 그 시점의 기록으로 유지하되 무효 범위를 명시합니다.

## 4. 검증

1. probe 통과 — 정책 단정이 새 기본값(미설정·`""`·`"yes"` → 켬 / `0`·`off`·`false` → 끔)을 담을 것.
2. 미설정 스모크: `backlog-enabled=true`, `coalesced=0`.
3. `=0` 스모크: `backlog-enabled=false`, `coalesced` > 0 (이전 동작 복원).
4. pumpit3 스모크: 회귀 없음.

## 5. 완료 기준

1. 기본 실행에서 `coalesced=0`이고 `tick_lag_ms`가 증가하지 않습니다.
2. opt-out으로 이전 동작이 복원됩니다.
3. 366 문서에 무효 범위가 적혔고, frontier 1‴b가 해소로 갱신됐습니다.

---

# Task 432 Work Order — make the tick backlog the default

## 1. Scope

Flip `REPIU_TIMER_TICK_BACKLOG` to default **on**. No new mechanism — Task 366's backlog simply
becomes the default path.

## 2. Files

The header's default comment and signature (`const char*`), with Task 366's measurement marked
**SUPERSEDED**; the resolver defaulting to true with only an explicit off disabling it; the
probe's policy assertion updated for the new default; frontier item 1‴b resolved; a scope note
at the head of Task 366's design and work log; and the census guide. **Not touched:** the
backlog cap of 64, safe-point count, injection policy, and the rendezvous spin.

## 3. Implementation rules

**Keep the off path** — `REPIU_TIMER_TICK_BACKLOG=0` is the regression control, and it is
because Task 366 left this switch in place that the diagnosis was possible. **Unrecognised
values resolve to on**, so a typo cannot silently restore the tick-losing path. Task 366's
figures are **not deleted**; they stay as the record of their moment with the scope of their
invalidity stated.

## 4. Verification

The probe must pass with the policy assertion covering the new default (unset, `""` and `"yes"`
on; `0`, `off`, `false` off); an unset smoke must report `backlog-enabled=true` with
`coalesced=0`; a `=0` smoke must restore `coalesced` above zero; and a pumpit3 smoke must show
no regression.

## 5. Done when

A default run shows `coalesced=0` with `tick_lag_ms` not growing, the opt-out restores the old
behaviour, and Task 366's documents carry the scope note while frontier item 1‴b reads resolved.
