# 20260906-617 Linux x64 zero return word RET site 추적 작업 지시

## 한국어

### 목적

`guest_source=0` direct RET event를 발생시킨 guest `RET` instruction의
주소를 특정하고, 그 결과를 다음 stack-writer 분석의 입력으로 남깁니다.

### 작업 항목

1. direct RET emitter가 guest site를 `R10D`에 설정하도록 합니다.
2. indirect-call emitter도 high-bit producer/site tag를 설정하도록 합니다.
3. x64 return thunk가 tag를 frame `status`로 전달하도록 유지합니다.
4. zero-source frame trace에 producer kind/site를 출력합니다.
5. Linux x64 Debug `repiu_core_probe`와 `repiu`를 빌드합니다.
6. `pumpit2a`를 실행해 zero-source RET site와 stack window를 수집합니다.
7. 확인된 RET site, expected fallthrough, zero stack word를 분석 문서와
   작업 로그에 기록합니다.

### 완료 기준

* core probe가 실패 없이 통과합니다.
* 기존 return target과 guest stack 동작이 변경되지 않습니다.
* `source=0` 로그에 direct RET producer site가 기록됩니다.
* zero-word를 기록한 stack writer 수정은 추측으로 적용하지 않고 후속
  작업으로 분리합니다.

### 제외 범위

* zero return target 보정
* resolver 또는 fault recovery 정책 변경
* guest 원본 바이트 패치
* RET site만으로 stack writer를 단정하는 동작 수정

## English

### Objective

Identify the guest `RET` instruction that produced the `guest_source=0` direct
RET event, leaving its evidence as input to the next stack-writer analysis.

### Work items

1. Set the guest site in `R10D` from the direct-RET emitter.
2. Set a high-bit producer/site tag from the indirect-call emitter.
3. Keep the x64 return thunk copying the tag into frame `status`.
4. Print producer kind and site in the zero-source frame trace.
5. Build Linux x64 Debug `repiu_core_probe` and `repiu`.
6. Run `pumpit2a` and capture the zero-source RET site and stack window.
7. Record the RET site, expected fallthrough, and zero stack word in the
   analysis and work log.

### Done criteria

* The core probe passes without failures.
* Existing return-target and guest-stack behavior remains unchanged.
* The `source=0` log identifies a direct-RET producer site.
* Any stack-writer fix is split into a following task rather than inferred from
  the RET site alone.

### Out of scope

* Correcting the zero return target
* Changing resolver or fault-recovery policy
* Patching original guest bytes
* Changing behavior based only on the identified RET site
