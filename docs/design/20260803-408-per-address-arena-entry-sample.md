# 20260803-408 주소별 arena 진입 표본 설계 / Per-Address Arena Entry Sample Design

## 한국어

### 배경

Task 407이 arena 진입 신호 둘을 찾았지만 **정상 모드에서 `0x0301DB22`가 진입하는
순간은 잡지 못했습니다.** 원인은 계측 구조입니다.

| 시도 | 보관 정책 | 결과 |
|---|---|---|
| 1차 | 선두 16건 | 부팅기에 전부 소진(overflow 11,596~13,635) |
| 2차 | 최신 16건(ring) | 정상 실행은 마지막 16건이 전부 PIC EOI, `0x0301DB22`가 채운 실행은 둘 다 격리 모드 |

**전역 버퍼로는 특정 주소를 겨냥할 수 없습니다.** 어떤 정책을 쓰든 가장 시끄러운
주소가 버퍼를 차지합니다.

### 설계

버퍼를 주소별로 나눕니다. Task 405 census는 이미 주소별 표이므로 **그 항목 안에
진입 표본 한 칸**을 넣으면 됩니다.

```
struct PortIoAddressCensusEntry
{
    std::uint32_t guest_address;
    std::uint32_t count;
    std::uint32_t cache_count;
    std::uint32_t mapped_count;
    std::uint32_t reentry_pending_count;
    // Task 408: 이 주소의 진입 전이. 첫 1건만 보관하고 횟수는 따로 센다.
    std::uint32_t entry_transition_count;
    std::uint32_t entry_previous_code;
    std::uint32_t entry_previous_eip;
    std::uint8_t  entry_flags;   // bit0 prev-in-cache, 1 tf, 2 reentry,
                                 // 3 legacy, 4 single-step
};
```

* 진입 판정은 Task 407과 동일합니다(`!from_aot_cache && prev_veh_code != 0xC0000096`).
* **첫 1건만 기록하고** 이후에는 `entry_transition_count`만 올립니다. 주소마다 칸이
  따로 있으므로 시끄러운 주소가 조용한 주소를 밀어내지 못합니다.
* 항목 30개 × 16바이트 남짓이며 기록은 주소가 이미 조회된 뒤이므로 추가 탐색이
  없습니다. **상시 ON**입니다.

### 첫 1건이 맞는 선택인 이유

`0x0301DB22`의 진입은 실행당 수천 회지만 **모두 같은 기전**일 것으로 예상됩니다.
Task 407의 두 신호가 각각 16건 내내 완전히 동일했던 것이 근거입니다. 만약 기전이
섞여 있다면 `entry_transition_count`와 첫 표본만으로는 알 수 없으므로, **그 경우
이 계측으로는 판정할 수 없다고 기록**하고 표본 수를 늘립니다.

### Task 407 전역 ring은 유지합니다

주소별 표본은 "이 주소가 어떻게 들어왔나"를, 전역 ring은 "마지막에 무슨 일이
있었나"를 답합니다. 서로 대체하지 않으므로 둘 다 남깁니다.

### 판정 기준

| `0x0301DB22`의 `entry_previous_code` | 뜻 | 다음 작업 |
|---|---|---|
| `0x80000003` + prev-in-cache | 캐시 INT3 이후 진입 | 그 INT3의 정체(Task 407 신호 a) |
| `0xC0000005` | AV 처리가 arena에 남김 | 그 AV 핸들러의 복귀 경로 |
| `0x80000004` | single-step 이후 | 재진입 실패 분기 |
| `entry_transition_count`가 `count`와 비슷 | 매 반복이 진입 | 진입 정의 재검토 |

마지막 행이 중요합니다. 지연 루프가 200회 연속 도는 동안 직전 예외는 계속
`0xC0000096`이므로 진입은 **200회에 1회꼴**이어야 합니다. 그보다 훨씬 크면 루프가
매 반복 다른 경로를 타고 있다는 뜻이므로 지금까지의 그림을 다시 봐야 합니다.

### 검증

* Release 빌드와 `repiu_aot_probe` 통과.
* pumpit3 45초 3회. 정상 모드(격리 없음) 실행이 최소 1회 포함돼야 하며, 없으면
  추가 실행합니다.
* Task 405/406/407 기존 값이 그대로인지 확인(동작 불변).
* pumpit1 회귀 1회.

### 이 Task가 하지 않는 것

진입을 막거나 복귀를 추가하지 않습니다.

---

## English

### Background

Task 407 found two arena-entry signatures but **did not capture the healthy-mode entry at
`0x0301DB22`**, for structural reasons: keeping the earliest sixteen let boot consume them all
(overflow 11,596-13,635), and keeping the newest sixteen gave the PIC EOI in healthy runs while
the runs whose ring filled with `0x0301DB22` were both quarantined. **A global buffer cannot
target one address** — whichever retention policy it uses, the noisiest address takes it.

### Design

Split the buffer by address. The Task 405 census is already a per-address table, so the entry
sample goes **inside each entry**: a transition count, the previous exception's code and EIP,
and a flag byte carrying prev-in-cache, trap flag, re-entry pending, legacy fallback, and
single-step trace.

The entry test is Task 407's, unchanged (`!from_aot_cache && prev_veh_code != 0xC0000096`).
**Only the first transition per address is stored** and later ones just increment the count, so
a noisy address cannot displace a quiet one. The cost is about sixteen bytes across thirty
entries, recorded after the address has already been looked up, so no extra search. **Always
on.**

### Why one sample is the right choice

Entries at `0x0301DB22` number in the thousands per run but are expected to share one
mechanism, on the evidence that each of Task 407's two signatures was identical across all
sixteen slots. If mechanisms turn out to be mixed, the count plus one sample cannot show it —
in that case **this instrument is recorded as unable to decide** and the sample count is
raised rather than the result stretched.

### Task 407's global ring stays

The per-address sample answers "how did this address get here" and the ring answers "what
happened most recently". They do not substitute for each other, so both remain.

### Decision rule

For `0x0301DB22`: a previous `0x80000003` with prev-in-cache means entry follows a cache INT3,
pointing at Task 407's signature (a); `0xC0000005` means an access-violation handler left
execution in the arena; `0x80000004` points at a failed re-entry branch. And if
`entry_transition_count` approaches `count`, the entry definition itself is wrong — the loop
runs 200 iterations with `0xC0000096` throughout, so entries should be roughly one per 200. A
much higher ratio would mean the loop takes a different path each iteration and the whole
picture needs revisiting.

### Verification

Release build and probe; three 45-second pumpit3 runs including at least one healthy
(non-quarantined) run, adding runs if none occurs; Task 405/406/407 values unchanged, proving
no behaviour change; and one pumpit1 regression run.

### Out of scope

Nothing blocks the entry or adds a return path.
