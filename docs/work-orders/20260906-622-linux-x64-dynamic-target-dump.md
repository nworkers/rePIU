# Task 622 작업 지시서: Linux x64 dynamic target dump

## 한국어

### 작업

1. `REPIU_AOT_DYNAMIC_TRACE=<guest-address>` parser와 opt-in 출력을
   추가합니다.
2. 지정 target의 dynamic append에서 raw guest bytes를 출력합니다.
3. translation-plan의 target instruction과 emitted image entry를 출력합니다.
4. `pumpit2a`와 core probe로 검증합니다.
5. `0x011A643A`가 실제 code인지 data-like bytes인지, 그리고
   `0x011A6440` fault가 어느 단계에서 생기는지 분석 문서와 작업 로그에
   기록합니다.

### 제한

* return target, dynamic acceptance, register state, memory protection 순서를
  변경하지 않습니다.
* trace가 없을 때 output과 emitted bytes를 변경하지 않습니다.
* target 주소에 대한 자동 보정이나 guest logic 재구현을 하지 않습니다.

### 완료 조건

* core probe가 `core_probe_failures=0`을 유지합니다.
* `0x011A643A`의 raw/plan/emitted evidence가 한 실행에서 출력됩니다.
* target resolution과 fault frontier가 진단 전후 동일합니다.

## English

### Work

1. Add an opt-in `REPIU_AOT_DYNAMIC_TRACE=<guest-address>` parser and output.
2. Print raw guest bytes for the selected dynamic append.
3. Print the target translation-plan instruction and appended image entry.
4. Verify with `pumpit2a` and the core probe.
5. Record whether `0x011A643A` contains code or data-like bytes and where the
   `0x011A6440` fault occurs in the analysis and work log.

### Limits

* Do not change the return target, dynamic acceptance, register state, or
  memory-protection ordering.
* Do not change output or emitted bytes when tracing is disabled.
* Do not auto-correct the target or reimplement guest logic.

### Done criteria

* The core probe keeps `core_probe_failures=0`.
* Raw, plan, and emitted evidence for `0x011A643A` appears in one run.
* Target resolution and the fault frontier remain unchanged with diagnostics.
