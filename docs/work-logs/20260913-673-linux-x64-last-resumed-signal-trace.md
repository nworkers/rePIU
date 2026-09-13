# Task 673 작업 로그: Linux x64 last resumed signal trace

## 한국어

마지막 signal/fault와 첫 low native RSP signal을 bounded 상태로 기록했습니다.
최초 low signal은 다음 guest 명령에서 확인되었습니다.

```text
first_low_rip=0x10efe62
first_low_eip=0x20001755
first_low_rsp=0x60ef1c7c
```

static census에서 해당 cache entry의 원본은 `83 C4 04`(`ADD ESP,4`)였고,
emitted bytes는 `41 83 C7 04`였습니다. 즉 x64 emitted 명령을 실행한 것이
아니라 non-identical 원본 bytes로 재진입한 것이 원인이었습니다.

## English

Added bounded state for the last resumed signal/fault and the first signal that
observed low native RSP. The first low signal was associated with:

```text
first_low_rip=0x10efe62
first_low_eip=0x20001755
first_low_rsp=0x60ef1c7c
```

The static census identified the original cache entry as `83 C4 04`
(`ADD ESP,4`) and its emitted form as `41 83 C7 04`. The failure therefore
came from reentering non-identical original bytes, not from executing the
translated x64 instruction.
