# 20260901-566 guest segment base 측정 작업 지시서

## 한국어

### 목적

Task 565가 남긴 질문 하나에 답합니다 — **guest의 segment base가 0인가.** x64에서
segment override를 어떻게 다룰지가 그 답에 달려 있습니다.

long mode는 `CS`/`DS`/`ES`/`SS` override를 무시하고 base를 0으로 봅니다. guest가 flat
model이면 prefix를 떼는 것이 곧 같은 의미이고, 아니면 i386처럼 base를 displacement에
접어 넣어야 합니다. **정반대의 두 결론이므로 재고 정합니다.**

### 작업

- census가 relocated image의 selector binding(선택자·base·limit·object)을 출력한다.

### 검증

Linux x64에서 census를 실행해 base 값을 기록합니다. i386과 Win32는 census를 빌드해
회귀로 확인합니다.

## English

### Objective

Answer the one question Task 565 left: **is the guest's segment base zero?** How x64
should handle segment overrides depends entirely on it.

Long mode ignores the `CS`/`DS`/`ES`/`SS` overrides and treats their base as zero. Under a
flat guest, dropping the prefix would mean the same thing; otherwise the base has to be
folded into the displacement as i386 does. **The two conclusions are opposites, so it is
measured rather than argued.**

### Work items

- Print the relocated image's selector bindings -- selector, base, limit, object -- from
  the census.

### Verification

Run the census on Linux x64 and record the bases. Build the census on i386 and Win32 as
regressions.
