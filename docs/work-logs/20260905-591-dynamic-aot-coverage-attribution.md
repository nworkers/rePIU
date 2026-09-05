# 작업 로그 20260905-591 — Dynamic AOT coverage 실패 주소 귀속

설계: [20260905-591](../design/20260905-591-dynamic-aot-coverage-attribution.md)  
작업 지시: [20260905-591](../work-orders/20260905-591-dynamic-aot-coverage-attribution.md)

## 결과

`AppendDynamicAotTranslation`이 coverage validator의 실패 guest 주소를 보존하도록
변경했습니다. 주소가 nonzero이면 result message는 `at 0xXXXXXXXX`을 붙이고, 주소가 없으면
기존 일반 메시지를 유지합니다. validator의 거절, image append 생략, resolver의 zero 반환,
return thunk의 INT3 fail-closed 동작은 바꾸지 않았습니다.

watched `pumpit2a`에서 `0x010F4AD1` dynamic append의 실제 coverage 실패 주소는
`0x010F4ACD`로 확인됐습니다.

```text
[repiu-x64-return] result=translation-failed source=0x010F4AD1 cache=0x00000000
    detail=dynamic AOT CFG lacks complete HLE/selector-guard coverage at 0x010F4ACD
```

## 검증

* WSL Linux x64 `repiu` 빌드 성공.
* `REPIU_GUEST_WATCH=0x010F4A96 REPIU_LINUX_X64_RETURN_TRACE=1 timeout 5s`
  watched `pumpit2a`에서 위 주소와 기존 `signal=0x5` 뒤 `signal=0x4` fail-closed 종료를
  확인했습니다. 외부 timeout의 종료 코드는 `124`였습니다.

## 후속 작업

`0x010F4ACD`의 planner record와 emitted image를 분석해, dynamic image가 요구하는
HLE/selector-guard coverage를 안전하게 완성해야 합니다.

---

# Work log 20260905-591 — Dynamic AOT coverage failure attribution

Design: [20260905-591](../design/20260905-591-dynamic-aot-coverage-attribution.md)  
Work order: [20260905-591](../work-orders/20260905-591-dynamic-aot-coverage-attribution.md)

## Result

`AppendDynamicAotTranslation` now preserves the coverage validator's nonzero
failing guest address in its result message. Validation rejection, skipped image
append, zero resolver result, and the return thunk's INT3 fail-closed path are
unchanged.

The watched `pumpit2a` append from `0x010F4AD1` failed coverage at
`0x010F4ACD`.

## Verification

* WSL Linux x64 `repiu` built successfully.
* A five-second watched `pumpit2a` run printed the failed address above and
  retained the existing signal-5 then signal-4 fail-closed termination; the
  outer timeout returned `124`.

## Follow-up

Inspect the planner record and emitted image at `0x010F4ACD`, then complete the
required coverage without bypassing validation or resuming raw guest bytes.
