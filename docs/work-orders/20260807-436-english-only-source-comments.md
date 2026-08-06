# Task 436 작업 지시 — 소스 주석은 영어만

## 1. 배경

`AGENTS.md`의 "문서는 한국어를 먼저 쓰고 영어 번역을 붙인다"는 규칙이 소스 주석에도
적용되는 것처럼 읽혀 왔고, 그 결과 코드에 한국어 주석과 한국어·영어 이중 언어 주석이
섞였습니다. 같은 설명을 두 벌 유지하면 **한쪽만 갱신되어 서로 어긋납니다.**

문서 규칙과 코드 규칙을 분리합니다. **문서는 그대로 한국어 우선 이중 언어, 소스 주석은
영어 한 벌.**

## 2. 범위

| 구분 | 처리 |
|---|---|
| 이중 언어 주석(한국어 + 영어) | 한국어 부분 삭제, 영어 유지 |
| 한국어 전용 주석 | 영어로 번역 |
| 문서(`docs/`, `README.md`, `ARCHITECTURE.md`, `AGENTS.md`) | **변경 없음** — 이중 언어 유지 |
| `third_party/` | **변경 없음** — 외부 코드 원본 유지 |

대상은 `src/`·`include/`의 C++ 소스와 헤더, 그리고 `scripts/`의 스크립트 주석입니다.

## 3. 규칙 문서화

* `docs/CODING_STYLE.md`에 "주석 언어 / Comment Language" 절을 추가합니다. 문서 규칙과
  다르다는 점, 적용 범위, 기존 파일 수정 시의 처리까지 명시합니다.
* `AGENTS.md` 핵심 원칙 4번에 이중 언어 규칙이 **문서 전용**임을 명시하고, 구현 규칙에
  영어 전용 주석 항목을 추가합니다(한국어·영어 양쪽).

## 4. 검증

1. `src/`·`include/`·`scripts/`에 한글 문자가 남아 있지 않습니다.
2. Win32 Debug 빌드가 통과합니다.
3. `repiu_aot_probe.exe`가 이전과 같은 결과로 통과합니다(주석만 바뀌었으므로 동작 불변).

---

# Task 436 Work Order — English-only source comments

## 1. Background

`AGENTS.md`'s rule that documents lead with Korean and add an English translation has been read
as covering source comments too, leaving the code with Korean comments and bilingual ones.
Maintaining the same explanation twice lets one copy be updated while the other drifts.

The document rule and the code rule are separated: **documents stay bilingual with Korean
first; source comments carry one English copy.**

## 2. Scope

Drop the Korean half of a bilingual comment, translate a Korean-only comment into English, and
leave documents (`docs/`, `README.md`, `ARCHITECTURE.md`, `AGENTS.md`) and `third_party/`
untouched. The targets are the C++ sources and headers under `src/` and `include/`, plus script
comments under `scripts/`.

## 3. Writing the rule down

`docs/CODING_STYLE.md` gains a "Comment Language" section stating how it differs from the
documentation rule, what it covers, and what to do on encountering a Korean comment while
editing. `AGENTS.md` marks core principle 4 as applying to documents only and adds an
English-only comment rule to the implementation rules, in both language sections.

## 4. Verification

No Hangul remains under `src/`, `include/` or `scripts/`; the Win32 Debug build passes; and
`repiu_aot_probe.exe` passes as before, since only comments changed.
