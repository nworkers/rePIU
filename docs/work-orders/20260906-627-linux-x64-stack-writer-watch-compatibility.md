# Task 627 작업 지시서: Linux x64 stack writer watch compatibility

## 한국어

### 작업

1. segment HLE의 watched-page 조건을 확인하는 보조 경로를 추가합니다.
2. 조건이 맞을 때만 store 전 protection을 writable로 전환하고 store 후
   복원합니다.
3. HLE store를 기존 guest write trace ring에 기록합니다.
4. 기본 trace 비활성 경로의 동작과 비용은 유지합니다.
5. core probe와 stack-page writer trace를 실행합니다.

### 제한

* guest selector, ESP, EIP, stack value semantics를 변경하지 않습니다.
* return target을 보정하거나 특정 writer를 정답으로 강제하지 않습니다.
* watched page 외의 memory protection을 변경하지 않습니다.

### 완료 조건

* `core_probe_failures=0`.
* stack-page watch가 HLE store에서 중단되지 않습니다.
* `0x0158CC44` target value의 native/HLE writer timeline이 관측됩니다.

## English

### Work

1. Add a watched-page check for the segment-HLE store.
2. Only for a match, make the page writable before the store and restore it
   afterward.
3. Record the HLE store in the existing guest write trace ring.
4. Preserve behavior and cost when tracing is disabled.
5. Run the core probe and stack-page writer trace.

### Limits

* Do not change guest selector, ESP, EIP, or stack-value semantics.
* Do not repair the return target or force a particular writer as the answer.
* Do not change memory protection outside the watched page.

### Done criteria

* `core_probe_failures=0`.
* Stack-page tracing continues through the HLE store.
* The native/HLE writer timeline for target `0x0158CC44` is observable.
