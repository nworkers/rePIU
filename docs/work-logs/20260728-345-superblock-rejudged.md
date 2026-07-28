# 20260728-345 작업 로그: 복귀 경로 수리 후 SUPERBLOCK 재판정 / Work log

## 한국어

### 결론 요약

**여전히 기각입니다. 다만 실패가 재현 가능해지고 한 명령으로 좁혀졌습니다.**

Task 338에서는 60초를 다 쓰며 Glide 초기화에서 멈췄고(gate 진입 74, 프레임 0),
지금은 **3회 모두 같은 주소에서 즉시 죽습니다.**

| 항목 | baseline (`SUPERBLOCK=0`) | `SUPERBLOCK=1` |
|---|---:|---:|
| 프레임 중앙값 | 3,325 | **없음(crash)** |
| Glide gate 진입 | 144,098~154,666 | 50 / 51 / 50 |
| LINEXE get-proc | 37 | 24 / 26 / 24 |
| 종료 사유 | 정상 timeout | `original entry raised a caught exception` (3/3) |

### 실패 지점

```
예외 0xC0000005, 주소 0x03042EBE (3회 모두 동일)
0x03042EBE: call far [0x012D9C90]
```

**`call far`입니다.** 그 포인터(`0x032D9C90`)는 baseline이 **INT 8 chain HLE**로
974회 처리하는 타이머 체인 진입점입니다. `SUPERBLOCK=1`에서는 그 far call이 캐시에서
**직접 실행되어 null 대상으로 점프**합니다.

**즉 exception-free HLE는 VEH가 반드시 매개해야 하는 far transfer를 native 코드로
내보냅니다.**

### 시도한 수정과 그 결과

`RequiresVehMediatedHle`에 far branch(`ZYDIS_BRANCH_TYPE_FAR`) 조건을 추가했습니다.
INT/IRET와 세그먼트/ESP 쓰기만 걸러내던 기존 목록이 far transfer의 **암묵적 CS 적재**를
놓치기 때문입니다(Zydis는 `call far`의 CS를 written operand로 보고하지 않습니다).

**결과: 실패는 그대로입니다.** 주소도 `0x03042EBE`로 동일합니다.

**따라서 원인은 런타임 thunk가 아니라 emit 시점 결정입니다.**
`RequiresVehMediatedHle`는 thunk에 **도달한** 명령에만 적용되는데, 이 far call은
애초에 dispatch site로 emit되지 않았습니다. 무엇을 inline dispatch로 만들지는
`enable_dbt_hle_dispatch`가 켜진 `BuildAotCodeCacheImage`가 정합니다.

**추가한 guard는 유지합니다.** 관측된 실패를 바꾸지는 않았지만 기존 INT/IRET 규칙과
같은 성격의 방어이며, 이 경로는 기본 OFF입니다. **다만 이 guard가 무엇을 고쳤다고
기록하지 않습니다.**

### 재판정이 필요했던 이유와 결과

Task 338의 실패는 quarantine된 페이지 때문일 가능성이 있었습니다(Task 342/344가 그것을
제거했습니다). **재판정 결과 그 가설은 기각됩니다.** quarantine이 0인 지금도
`SUPERBLOCK`은 실패하며, 실패 원인은 별개입니다.

### 확인됨 / Confirmed

* `SUPERBLOCK=1`은 여전히 사용할 수 없습니다. 3/3 재현되는 `0xC0000005`입니다.
* 실패 명령은 `0x03042EBE`의 `call far [0x012D9C90]`이며 INT 8 chain HLE 진입점입니다.
* 원인은 emit 시점 결정이며 런타임 thunk 술어가 아닙니다.
* quarantine이 원인이라는 가설은 기각됐습니다.

### 미확정 / Unresolved

* emitter가 이 far call을 어떤 kind로 분류해 native로 내보내는지 확인하지 않았습니다.
  다음 작업의 첫 질문입니다.
* far transfer 외에 같은 방식으로 새는 명령 종류가 더 있는지.
* 따라서 `SUPERBLOCK`은 **emitter 측 계약이 정리되기 전에는 재판정 대상이 아닙니다.**

---

## English

`SUPERBLOCK=1` is still rejected, but the failure is now reproducible and localised. Task 338 saw
it spend the whole 60 seconds stalled in Glide initialisation; all three runs now die immediately
at the same address, with 50-51 gate entries, 24-26 resolved procs, and no frames, against a
baseline median of 3,325 frames and 37 procs.

The crash is `0xC0000005` at `0x03042EBE` in every run, which disassembles as
`call far [0x012D9C90]` — the timer chain entry the baseline handles 974 times as INT 8 chain HLE.
Under `SUPERBLOCK` that far call executes from the cache and jumps to a null target, so
exception-free HLE emits as native code a far transfer that must stay VEH-mediated.

Adding `ZYDIS_BRANCH_TYPE_FAR` to `RequiresVehMediatedHle` — the existing list catches INT, IRET,
and segment or ESP writes but misses a far transfer's implicit CS load, which Zydis does not report
as a written operand — did not change the failure or its address. The cause is therefore an
emit-time decision rather than the runtime thunk predicate: `RequiresVehMediatedHle` only applies
to instructions that reach the thunk, and this far call was never emitted as a dispatch site. The
guard is kept as defence in depth of the same character as the INT/IRET rule, on a path that is off
by default, but it is not credited with fixing anything.

The re-judgement was warranted because Task 338's failure might have been caused by the quarantined
pages that Tasks 342 and 344 removed. That hypothesis is rejected: quarantine is now zero and
`SUPERBLOCK` still fails for an unrelated reason. Unresolved: how the emitter classifies this far
call, whether other instruction kinds leak the same way, and consequently that `SUPERBLOCK` is not
a candidate for re-judgement again until the emitter-side contract is settled.
