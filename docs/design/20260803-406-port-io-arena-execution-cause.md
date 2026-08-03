# 20260803-406 Port I/O arena 실행 원인 설계 / Why Port I/O Runs in the Arena

## 한국어

### 배경

Task 405가 확정한 것은 **`0x0301DB22`가 AOT 캐시가 아니라 guest arena에서 실행된다**는
사실입니다. census의 `cache_count`가 모든 실행·모든 항목에서 0이고, 이 명령 하나가
port I/O의 85.9~97.2%이며, 그 예외의 VEH gap이 wall의 41.9~49.7%입니다.

### 코드 읽기로 배제한 것

* **excluded range 아님.** `aot_excluded_guest_ranges`에 들어가는 것은 LINEXE gate code
  하나뿐입니다(`execution_trampoline.cpp:4114`). `0x0301D000`대는 포함되지 않습니다.
* **분류기 문제 아님.** `ReadGuardedPortIoInstruction`은 `IN`/`OUT`에 `DX` 피연산자가
  있으면 참이며 `66 ED`도 해당합니다.
* **emitter 문제 아님.** `kPortIo`는 `EmitHleDispatchSlot`으로 가고 그 함수는 항상
  true를 반환합니다. 계획된 블록 안의 port I/O는 raw `in`으로 방출될 수 없습니다.

### 남은 두 가설

같은 실행의 카운터가 서로 다른 방향을 가리켜 코드 읽기로는 결론이 나지 않습니다.
`AOT entry/boundary/reentry/fallback = 1/44,890/81,079/0`은 캐시가 활발히 쓰인다고
말하는데, port I/O 예외는 1,034,948건이고 그중 캐시 실행은 0건입니다.

| 가설 | 뜻 | 고칠 곳 |
|---|---|---|
| **C. 번역 부재** | 그 주소에 캐시 매핑이 아예 없다 | 번역 커버리지 |
| **D. 복귀 안 함** | 매핑은 있는데 실행이 arena에 남는다 | 재진입 정책 |

### 설계

Task 405 census에 필드 둘을 더합니다. 판정에 필요한 최소량입니다.

```
struct PortIoAddressCensusEntry
{
    std::uint32_t guest_address = 0;
    std::uint32_t count = 0;
    std::uint32_t cache_count = 0;      // Task 405
    std::uint32_t mapped_count = 0;     // Task 406: 캐시 매핑이 존재했는가
    std::uint32_t reentry_pending_count = 0;  // Task 406: 복귀가 예약돼 있었는가
};
```

* `mapped_count`는 `FindAotCacheAddress(*context->aot_placement, decode_eip, &unused)`의
  결과입니다. **arena에서 실행 중이더라도 번역이 존재하는지**를 봅니다.
* `reentry_pending_count`는 `context->aot_reentry_pending`을 그대로 읽습니다.
  Task 405에서 reentry funnel 44,589건이 port I/O 예외 1,034,948건보다 훨씬 적었던
  이유가 여기서 드러납니다.
* 기록 위치는 Task 405와 같은 한 곳입니다.

**비용 주의:** `FindAotCacheAddress`는 Task 324가 해시 색인으로 바꾼 뒤 호출당 약
6,866 tick입니다. 초당 약 23,000회면 초당 약 158M tick, 2.7GHz 기준 **약 5.8%** 로
무시할 수 없습니다. 따라서 이 두 필드는 **`REPIU_PORT_IO_CENSUS_MAPPING`이 설정될 때만**
채우고 기본은 OFF입니다. Task 405의 `count`/`cache_count`는 상시 ON 그대로입니다.

측정 왜곡을 피하려면 이 스위치를 켠 실행의 **절대 시간·프레임 수는 인용하지 않고**,
비율만 읽습니다.

### 판정 기준

| 관측 | 결론 | 다음 작업 |
|---|---|---|
| `mapped_count` ≈ 0 | 가설 C | 그 블록이 왜 번역되지 않는가 |
| `mapped_count` ≈ `count` | 가설 D | 왜 캐시로 복귀하지 않는가 |
| `reentry_pending_count` ≈ 0 | 복귀가 **예약조차** 안 됨 | 예약 조건을 본다 |

### 검증

* Release 빌드와 `repiu_aot_probe` 통과.
* 스위치 OFF에서 Task 405 수치가 재현되는지(동작 불변).
* 스위치 ON pumpit3 45초 실행에서 두 필드를 읽습니다.
* pumpit1 회귀 1회.

### 이 Task가 하지 않는 것

번역 커버리지도 재진입 정책도 바꾸지 않습니다.

---

## English

### Background

Task 405 established that **`0x0301DB22` executes in the guest arena rather than the AOT
cache**: `cache_count` is zero in every entry of every run, that one instruction is 85.9-97.2%
of all port I/O, and the resulting VEH gap is 41.9-49.7% of wall.

### Ruled out by reading the code

It is not an excluded range — the only entry pushed into `aot_excluded_guest_ranges` is the
LINEXE gate code at `execution_trampoline.cpp:4114`. It is not a classifier problem, since
`ReadGuardedPortIoInstruction` accepts any `IN`/`OUT` with a `DX` operand, `66 ED` included.
And it is not an emitter problem, because `kPortIo` routes to `EmitHleDispatchSlot`, which
always returns true, so a port I/O instruction inside a planned block cannot be emitted as a
raw `in`.

### Two hypotheses remain

Counters from the same run point in different directions, so reading further will not settle
it: `AOT entry/boundary/reentry/fallback = 1/44,890/81,079/0` says the cache is in active
use, while 1,034,948 port I/O exceptions occurred with zero of them in cache code. Either no
mapping exists for that address (**C**, a coverage problem) or a mapping exists and execution
stays in the arena anyway (**D**, a re-entry policy problem).

### Design

Add two fields to the Task 405 census — the minimum that separates C from D.
`mapped_count` records whether `FindAotCacheAddress` finds a translation for the address even
while executing in the arena; `reentry_pending_count` records `aot_reentry_pending`, which
should explain why the re-entry funnel saw only 44,589 attempts against 1,034,948 port I/O
exceptions. Both are filled at the same single site as Task 405.

**Cost note:** `FindAotCacheAddress` costs about 6,866 ticks per call since Task 324's hash
index. At roughly 23,000 calls per second that is about **5.8% of a 2.7 GHz core**, which is
not negligible, so these two fields are filled **only when `REPIU_PORT_IO_CENSUS_MAPPING` is
set**, default off. Task 405's `count` and `cache_count` stay always on. Runs with the switch
enabled must be read for ratios only — **their wall time and frame counts are not quotable**.

### Decision rule

`mapped_count` near zero means hypothesis C and the next question is why that block is never
translated; `mapped_count` near `count` means hypothesis D and the next question is why
execution does not return to the cache; and `reentry_pending_count` near zero means the return
is never even scheduled, which points at the scheduling condition.

### Verification

Release build and probe; Task 405's figures reproduced with the switch off, proving no
behaviour change; one 45-second pumpit3 run with it on to read the two fields; and one
pumpit1 regression run.

### Out of scope

Neither translation coverage nor the re-entry policy is changed.
