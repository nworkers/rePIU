# Task 676 작업 로그: Linux x64 stack high-byte destination

## 한국어

`MOV AH/CH/DH/BH,[ESP+disp]` 형태를 generic lowering 대상으로 추가했습니다.
source memory는 `R15D`를 사용하고, 임시 `DL`을 통해 high-byte destination을
기록한 뒤 `DL`을 복원합니다. REX가 high-byte register 이름을 바꾸는 문제를
주소별 예외 없이 처리합니다.

`8A 64 24 2C`의 기대 emitted bytes는 다음과 같습니다.

```text
44 88 F2 41 8A 54 27 2C 8A E2 44 88 F2
```

positive/negative lowering probe와 core probe 27/27을 통과했습니다.

## English

Added generic lowering for `MOV AH/CH/DH/BH,[ESP+disp]`. The source memory
uses `R15D`; a temporary `DL` carries the loaded byte into the legacy high-byte
destination, then `DL` is restored. This handles the REX high-byte naming change
without an address-specific exception.

Expected emitted bytes for `8A 64 24 2C` are:

```text
44 88 F2 41 8A 54 27 2C 8A E2 44 88 F2
```

Positive/negative lowering probes and all 27 core probes passed.
