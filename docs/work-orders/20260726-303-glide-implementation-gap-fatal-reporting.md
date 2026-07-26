# 20260726-303 작업 지시: Glide 미구현 fatal 보고 / Work order

설계: [20260726-303-glide-implementation-gap-fatal-reporting.md](../design/20260726-303-glide-implementation-gap-fatal-reporting.md)

## 한국어

### 목표

Glide 미구현 함수와 미지원 인자를 고유 함수·인자 조합으로 누적하고 `FATAL`로 명확히
출력하되, ABI가 검증된 호출은 안전 반환 후 계속 실행합니다.

### 구현 순서

1. 플랫폼 공용 issue 분류, record, bounded tracker를 추가합니다.
2. tracker 합성 probe와 CMake target을 추가합니다.
3. Win32 Glide boundary에 최초 고유 issue 즉시 출력 adapter를 연결합니다.
4. default, safe decline, retain, 명시적 ABI no-op, hard reject를 분류합니다.
5. execution attempt snapshot에 tracker 결과를 전달합니다.
6. loader 종료 요약에 분류별 total과 모든 고유 record를 출력합니다.
7. 아키텍처·누적 analysis를 갱신합니다.
8. probe, Win32 x86 Debug 빌드, pumpit1 smoke를 검증하고 작업 로그를 남깁니다.

### 완료 조건

- 미구현 함수/미지원 인자의 최초 고유 조합이 즉시 `FATAL`로 출력됩니다.
- 같은 조합의 반복은 summary count로 합쳐집니다.
- 다른 인자 조합은 별도 record입니다.
- 종료 summary에 total/unique/overflow와 record가 남습니다.
- known-ABI 미구현 호출 이후 실행이 계속됩니다.
- hard ABI reject는 기존 종료 정책을 유지합니다.

---

## English

Track unique Glide unimplemented-function and unsupported-argument combinations
and report them explicitly as `FATAL`, while preserving Task 302 safe continuation
for trusted ABI calls. Add the shared bounded tracker and synthetic probe,
instrument all known implementation-gap paths, carry observations into the
execution attempt, and print totals plus every unique record in the loader
summary. Build and smoke verification must prove clear fatal reporting without
turning known-ABI gaps back into process termination.
