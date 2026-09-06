# Task 626 작업 지시서: Linux x64 return frame register provenance

## 한국어

### 작업

1. Task 626 설계에 맞춰 선택 주소 parser와 register trace helper를
   추가합니다.
2. `LinuxX64EngineResolver`에서 `frame->guest_source`가 선택 주소일 때
   frame guest GPR, ESP, EFLAGS, producer, continuation 정보를 기록합니다.
3. trace가 비활성화된 기본 경로의 실행 semantics와 출력은 유지합니다.
4. core probe와 Linux x64 재현을 실행하고 EAX provenance를 판정합니다.
5. 결과를 `docs/analysis/linux-port-frontier.md`와 작업 로그에 기록합니다.

### 제한

* guest register, EIP, EFLAGS, stack, cache map, resolver policy를 변경하지
  않습니다.
* 선택한 return target 외에는 register trace를 출력하지 않습니다.
* trace를 이유로 fallback, translation, HLE 정책을 변경하지 않습니다.

### 완료 조건

* `core_probe_failures=0`.
* `0x011A643A` return frame의 register/frame state가 재현 로그에 보입니다.
* 기존 fault frontier의 변화 여부와 EAX가 thunk 진입 전에 이미 정해졌는지
  판정됩니다.

## English

### Work

1. Add the selected-address parser and register-trace helper from the design.
2. When `frame->guest_source` matches, record frame guest GPRs, ESP, EFLAGS,
   producer, and continuation fields in `LinuxX64EngineResolver`.
3. Preserve the default execution semantics and output when tracing is disabled.
4. Run the core probe and Linux x64 reproduction and determine EAX provenance.
5. Record the result in `docs/analysis/linux-port-frontier.md` and the work log.

### Limits

* Do not change guest registers, EIP, EFLAGS, stack, cache mapping, or resolver
  policy.
* Do not print the register trace for return targets other than the selected
  address.
* Do not change fallback, translation, or HLE policy because of the trace.

### Done criteria

* `core_probe_failures=0`.
* The `0x011A643A` return frame's register/frame state is visible in the
  reproduction log.
* Determine whether the fault frontier changes and whether EAX was already
  fixed before thunk entry.

## 결과 / Result

* return resolver frame에서 `EAX=0x00000000`을 확인했습니다.
* 일반 `RET`가 `0x0158CC44`의 `0x011A643A`를 target으로 소비했습니다.
* 동적 fragment의 `OR EAX,0x37016BE9`가 fault 직전 EAX를 만든 것으로
  확인했습니다.
* 최종 fault frontier는 `0x011A6440`으로 유지되었습니다.

## Result (English)

* The return resolver frame had `EAX=0x00000000`.
* An ordinary `RET` consumed `0x011A643A` from stack slot `0x0158CC44`.
* The dynamic fragment's `OR EAX,0x37016BE9` was confirmed as the source of
  the EAX value immediately before the fault.
* The final fault frontier remained `0x011A6440`.
