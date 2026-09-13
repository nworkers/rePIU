# Task 670 작업 로그: Linux x64 return thunk host register trace

## 한국어

Linux signal fault report에 host `R10`, `R14`, `R15`를 추가하고 return thunk
진입 전후의 `RSP`를 함께 기록했습니다. 실행 의미는 변경하지 않은 진단 작업입니다.

검증은 Linux x64 `repiu` 빌드로 완료했습니다.

## English

Added host `R10`, `R14`, and `R15` to the Linux signal fault report, together
with `RSP` observations at guest entry, cache-call, and return-thunk boundaries.
This was diagnostic-only and changed no execution semantics.

The Linux x64 `repiu` target built successfully.
