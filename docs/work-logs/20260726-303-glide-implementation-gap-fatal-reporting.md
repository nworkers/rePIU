# 20260726-303 작업 로그: Glide 구현 공백 fatal 보고 / Work log

설계: [20260726-303-glide-implementation-gap-fatal-reporting.md](../design/20260726-303-glide-implementation-gap-fatal-reporting.md)

작업 지시: [20260726-303-glide-implementation-gap-fatal-reporting.md](../work-orders/20260726-303-glide-implementation-gap-fatal-reporting.md)

## 한국어

### 결과

Glide 구현 공백을 플랫폼 공용 `GlideImplementationIssueTracker`로 분류·누적하도록
변경했습니다. 분류는 다음과 같습니다.

| 분류 | 로그 등급 | 실행 정책 |
|---|---|---|
| `GLIDE_UNIMPLEMENTED_FUNCTION` | fatal | 검증된 ABI는 계속 |
| `GLIDE_UNSUPPORTED_ARGUMENT` | fatal | 보수적 반환 후 계속 |
| `GLIDE_BACKEND_FAILURE` | error | 보수적 반환 후 계속 |
| `GLIDE_ABI_REJECT` | fatal | 기존 hard reject |

같은 분류·ordinal·이름·이유·argument byte count·첫 8개 인자는 하나의 record로
병합하고 count를 증가시킵니다. 고유 record는 128개로 제한하지만 분류별 total과
overflow count는 계속 누적합니다.

### 로그

catalog default, Task 302 safe decline, 미지원 combine/blend retain, 명시적
texture/draw/LFB no-op, texture table/download/source 및 LFB lock/unlock/write의
부분 구현 실패, signature/반환 주소 reject를 추적기에
연결했습니다. 합성 probe는 반복 count 2를 포함한 다음 exact line도 검증합니다.

```text
[repiu-fatal] GLIDE_UNIMPLEMENTED_FUNCTION action=continue ordinal=17 name=_GRUNIMPLEMENTED@4 reason=catalog-default-handler detail="ABI-preserving default" argument_bytes=4 count=2 args=0x00000003
```

실행 종료 시에는 분류별 total/unique/overflow를 먼저 출력하고, 각 record를
`critical/FATAL` 또는 `error`로 반복 출력합니다. 실시간 경계, 종료 요약, 합성
probe가 동일한 공용 formatter를 사용합니다.

### 검증

- `repiu_glide_issue_probe`: exit 0, `glide_issue_probe=pass`
- 검증 범위: 중복 병합, 다른 인자 분리, fatal 판정, 분류별 total, 128-record
  overflow, 위 exact log line
- Win32 x86 Debug `repiu_glide_issue_probe`와 `repiu_loader_win32`: 빌드 성공
- 기존 C4819와 LNK4217 경고 외 compile/link 오류 없음
- 최종 30초 `pumpit1` smoke: process exit 0, 정상 timeout, progress 528,240,
  Glide gate 49/49, issue total `0/0/0/0`, unique/overflow `0/0`
- `git diff --check`: 통과

현재 환경에서는 60초 smoke도 초기 49개 Glide gate에 머물러 실제 콘텐츠 단계의
fatal line까지 도달하지 않았습니다. 사용자 로그에서는 이후 texture sampler/combine와
`_GRHINTS@8` 호출이 확인되므로, 장시간 새 빌드에서는 해당 첫 함수·인자 조합부터
즉시 fatal이 남습니다. known-ABI 경로는 `action=continue`이며 process를 종료하지
않습니다.

### 부수 정리

8개 인자를 받는 `_GRLFBWRITEREGION@32`가 8-dword stack mirror의 범위를 벗어난
`glide_gate_stack[8]`을 읽던 부분을 guest stack 직접 캡처의 8번째 인자로
교체했습니다.

---

## English

### Result

Added the platform-neutral `GlideImplementationIssueTracker`. Unimplemented
functions and unsupported arguments are fatal diagnostics, backend failures
are errors, and ABI rejects remain fatal hard rejects. Known-ABI gaps still
return conservatively with normal stdcall cleanup and continue.

Records coalesce identical category/ordinal/name/reason/argument-byte-count/
first-eight-argument combinations and retain repeat counts. Storage is bounded
at 128 unique records while per-category totals and overflow continue.

Instrumented catalog defaults, Task 302 safe declines, unsupported
combine/blend retain paths, explicit texture/draw/LFB no-ops, partial
texture-table/download/source and LFB lock/unlock/write failures, and
signature/return-address rejects. Immediate boundary
output, final critical/error summaries, and the synthetic probe share one exact
formatter.

The synthetic probe passed exact fatal-line formatting, deduplication, argument
separation, fatal classification, totals, and overflow with exit 0. Win32 x86
Debug probe and loader targets built successfully with only existing C4819 and
LNK4217 warnings. The final 30-second `pumpit1` smoke ended by normal timeout
with process exit 0, progress 528,240, 49/49 Glide gates, and zero issue totals.
`git diff --check` passed.

This environment still remained at the initial 49 gates after 60 seconds, so
the real content-phase immediate/final count comparison remains a longer-run
check. The user log confirms later texture sampler/combine and `_GRHINTS@8`
calls; the rebuilt boundary will report their first unique combinations
immediately. Those known-ABI records use `action=continue` and do not terminate
the process.

Also replaced the out-of-bounds `glide_gate_stack[8]` read for the eight-argument
`_GRLFBWRITEREGION@32` call with the eighth value from direct guest-stack
capture.
