# 작업 지시 20260905-591 — Dynamic AOT coverage 실패 주소 귀속

설계: [20260905-591](../design/20260905-591-dynamic-aot-coverage-attribution.md)

## 변경

Dynamic AOT append의 coverage validator가 제공한 실패 guest 주소를 result message에
포함합니다. 주소가 없으면 기존 일반 메시지를 유지하고, validator·append·resolver의
fail-closed 정책은 변경하지 않습니다.

## 검증

1. Linux x64 `repiu` 빌드
2. watched `pumpit2a` return trace에서 `0x010F4AD1` failure address 확인
3. analysis와 작업 로그 갱신 후 커밋

---

# Work order 20260905-591 — Dynamic AOT coverage failure attribution

Design: [20260905-591](../design/20260905-591-dynamic-aot-coverage-attribution.md)

## Change

Add the failed coverage guest address to the dynamic-append result message while
retaining the current validator, append, resolver, and fail-closed behavior.

## Verification

1. Build Linux x64 `repiu`.
2. Confirm the `0x010F4AD1` failure address in watched `pumpit2a` return trace.
3. Update analysis and work log, then commit.
