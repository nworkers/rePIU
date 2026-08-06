# Task 436 작업 로그 — 소스 주석을 영어 전용으로 정리

작업 지시: [20260807-436](../work-orders/20260807-436-english-only-source-comments.md)

## 1. 규칙을 먼저 갈라 두었습니다

`AGENTS.md`의 이중 언어 규칙이 코드까지 덮는 것처럼 읽혀 온 것이 원인이므로, 규칙
문서에서 두 대상을 분리했습니다.

| 문서 | 추가·수정 |
|---|---|
| `docs/CODING_STYLE.md` | "주석 언어 / Comment Language" 절 신설. 문서 규칙과 다르다는 점, 적용 범위(`src`·`include`·`scripts`·빌드 파일, `third_party/` 제외), 기존 파일 수정 시 처리 |
| `AGENTS.md` 핵심 원칙 4 | 이중 언어 규칙이 **문서 전용**임을 명시 |
| `AGENTS.md` 구현 규칙 | 영어 전용 주석 항목 추가, 세부는 `CODING_STYLE.md` 참조 |

## 2. 기존 주석 정리

한글이 있던 코드 파일은 13개(C++ 12, PowerShell 1)였습니다.

| 형태 | 처리 | 파일 |
|---|---|---|
| 이중 언어(한국어 + 영어) | 한국어 삭제 | `env_toggle.h`, `execution_backend.h`, `execution_timeout.h`, `dos_file_system.cpp`, `main.cpp`, `execution_timeout.cpp` |
| 한국어 전용 | 영어로 번역 | `env_toggle.cpp`, `native_linear_span.cpp`, `env_toggle_probe.cpp`, `execution_backend_probe.cpp`, `execution_timeout_probe.cpp`, `native_linear_span_probe.cpp`, `benchmark_native_linear_span.ps1` |

이중 언어 블록 제거는 스크립트로 처리했습니다 — 연속된 `//` 블록이
`[한국어][빈 `//`][영어]` 형태일 때만 앞부분을 지우고, 나머지는 수동 처리 목록으로
보고하게 해서 영어 원문이 없는 주석이 조용히 삭제되지 않도록 했습니다. 실제로 6개
파일이 그 목록으로 넘어와 손으로 번역했습니다.

**문서는 건드리지 않았습니다.** `docs/`·`README.md`·`ARCHITECTURE.md`·`AGENTS.md`는
계속 한국어 우선 이중 언어입니다.

## 3. 검증 (2026-08-07, Win32 x86 Debug)

| 검증 | 결과 |
|---|---|
| 추적 파일 전수 한글 스캔 | 코드에는 **0건**. 남은 것은 문서 4개와 `tests/history/`의 과거 HTML 스냅샷 2개(기록물이라 보존) |
| Debug 빌드 | **exit 0** |
| `repiu_aot_probe.exe MASTER\PIU_1ST\PIU\PIU.EXE` | **exit 0**, `execution_backend_policy=true`, `execution_timeout_policy=true` |

부수 효과로 정리 전 여러 건이던 MSVC **C4819**(코드 페이지 949에서 표현 불가한 문자)
경고가 **2건**으로 줄었습니다 — 외부 `spdlog` 헤더 1건과 우리 헤더
`glide_setter_state_model.h` 1건입니다.

## 4. 남은 것 — 영어 주석 안의 비ASCII 문자

우리 코드에 남은 C4819의 원인은 한글이 아니라 **영어 주석에 쓰인 비ASCII 문장부호**
입니다. 코드 파일 14개에 em dash(U+2014) 21개, 절 기호 `§`(U+00A7) 3개,
`timer_interrupt_boundary.h`의 BOM 1개가 있습니다. 언어 규칙 위반은 아니므로 이번
범위에서 제외했습니다. ASCII로 치환하면 우리 코드의 C4819는 0이 됩니다.

---

# Task 436 Work Log — converting source comments to English only

## 1. Splitting the rule first

The cause was `AGENTS.md`'s bilingual rule reading as though it covered code, so the rule
documents now separate the two. `docs/CODING_STYLE.md` gains a "Comment Language" section
covering how it differs from the documentation rule, what it applies to (`src`, `include`,
`scripts` and build files, excluding `third_party/`), and what to do when editing a file that
still has Korean comments. `AGENTS.md` marks core principle 4 as applying to documents only and
adds the English-only comment rule to its implementation rules.

## 2. Converting the existing comments

Thirteen code files contained Hangul — twelve C++ and one PowerShell. Bilingual comments in
`env_toggle.h`, `execution_backend.h`, `execution_timeout.h`, `dos_file_system.cpp`, `main.cpp`
and `execution_timeout.cpp` lost their Korean half; Korean-only comments in `env_toggle.cpp`,
`native_linear_span.cpp`, the four probe sources and `benchmark_native_linear_span.ps1` were
translated.

The bilingual removal ran through a script that only strips a `//` block shaped
`[Korean][bare //][English]` and **reports everything else for manual handling**, so no comment
without an English counterpart could be deleted silently. Six files came back on that list and
were translated by hand.

**Documents were not touched:** `docs/`, `README.md`, `ARCHITECTURE.md` and `AGENTS.md` remain
bilingual with Korean first.

## 3. Verification (2026-08-07, Win32 x86 Debug)

A scan of every tracked file finds **no Hangul in code**; what remains is the four bilingual
documents and two historical HTML snapshots under `tests/history/`, which are kept as recorded
evidence. The Debug build exits 0, and `repiu_aot_probe.exe` against
`MASTER\PIU_1ST\PIU\PIU.EXE` exits 0 with `execution_backend_policy=true` and
`execution_timeout_policy=true`.

As a side effect the MSVC **C4819** warnings (characters unrepresentable in code page 949) drop
to **two**: one in the external `spdlog` header and one in our own
`glide_setter_state_model.h`.

## 4. Left open — non-ASCII punctuation inside English comments

What still raises C4819 in our own code is not Korean but **non-ASCII punctuation in English
comments**: 21 em dashes (U+2014) and three section signs `§` (U+00A7) across fourteen code
files, plus a BOM in `timer_interrupt_boundary.h`. None of that breaks the language rule, so it
was left outside this task's scope; replacing them with ASCII would take our C4819 count to
zero. New comments follow the comment-language section of `docs/CODING_STYLE.md`.
