# Task 249 작업 로그 — pumpit1 10분 관측과 Glide HLE 전체 분석 / Task 249 Work Log — 10-Minute pumpit1 Observation and Full Glide HLE Analysis

* 날짜 / Date: 2026-07-19
* 브랜치 / Branch: `feature/249-glide-render-path-analysis`
* 범위 / Scope: 분석·설계만 (제품 코드 변경 없음) / analysis and design only (no product code change)

## 수행 내용 / What Was Done

1. **10분(600초) 관측 구동:** `cmd /c` 리다이렉션으로
   `REPIU_EXECUTION_BACKEND=aot-dynamic` supervisor `pumpit1 600000` 구동.
   결과: 600초 완주, fatal 0, 거부 게이트 0, child_exit=124(설계된 강제 종료),
   dispatch_entry 5,287,600, progress 286,238. 국면: 부팅(~15초) → 텍스처 기술자
   파싱 루프(15~77초) → **프레임 루프 정착(78초~종료)**.
2. **게이트 전수 로그 분석:** 초기화 시퀀스 96건 확인(§관측은
   `docs/analysis/glide2x-ovl-and-opengl-hle.md` Task 249 절). 1 Hz 샘플 560여 건
   에서 draw(66~77)/LFB(110~117) ordinal 0건.
3. **Glide HLE 3계층 인벤토리:** `linexe_glide_boundary.cpp`(게이트 44개 분기),
   `glide_hle.{h,cpp}`(카탈로그 44개·논리 상태), `glide_opengl_backend.cpp`/
   `glide_opengl_shader.cpp`(창/컨텍스트/상태·GLSL combine). 처리 등급 A(실의미)/
   B(부분)/C(상태만)/D(no-op) 분류 — 설계 문서 §3.
4. **export 격차 확정:** `PIU.EXE` 문자열 스캔으로 장식된 Glide 이름 97개,
   `glide2x.ovl` resident-name table 전수 파싱으로 ordinal 0~172 확정.
   카탈로그 44개 대비 **53개 미등록**.
5. **검은 화면 근인 확정:** 제시(grBufferSwap no-op)/그리기(clear·draw no-op)/
   텍스처(다운로드 폐기) 3계층 전면 no-op — 구조적 결과로 확정.
6. **보완 방안 설계:** `docs/design/20260719-249-glide-render-path-completion.md`
   신규 작성 — R0 게이트 안전망(97개 카탈로그+기본 핸들러+ordinal 라이브 카운트) →
   R1 프레임 제시 → R2 정점 → R3 텍스처 → R4 LFB → R5 충실도.
7. **문서 갱신:** `current-execution-frontier.md`에 Task 249 항목 추가,
   `glide2x-ovl-and-opengl-hle.md`에 관측·export 인벤토리 추가 및 Task 234의
   낡은 cdecl 결론을 Task 235 근거로 정정.

## 부수 발견 / Side Findings

* **timeout-teardown segfault 재확인(exit 139, Task 235 잔여):** 로더 자체
  타임아웃(150초) 직접 구동이 종료 요약(ordinal별 호출 카운트) 출력 전에 죽음.
  ordinal 카운트 검증 루프의 선결 과제로 설계 문서 §6에 기록.
* 빌드 트리는 `build/win32_x86_debug`(구 `win32_x86_dpmi` 트리는 소멸).

## 검증 / Verification

코드 변경이 없으므로 빌드 검증 대상 없음. 관측 재현 절차: 위 1번 명령. 로그:
세션 스크래치패드 `pumpit1-600s.log`(600초), `pumpit1-150s-summary.log`(150초,
teardown segfault 재현).

## English Summary

Ran `pumpit1` for the requested 10 minutes on aot-dynamic (600 s completed,
no fatal, zero rejected gates; frame loop stable from 78 s). Confirmed the black
screen is structural: present/draw/texture layers of the Glide HLE are all
ABI-preserving no-ops, so nothing is ever presented after the single black clear
at window open. Inventoried all three HLE layers, pinned the export gap (97
referenced names vs 44 cataloged, 53 unregistered), and wrote the phased
completion design (R0 gate safety net → R1 present → R2 vertex → R3 texture →
R4 LFB → R5 fidelity) in `docs/design/20260719-249-glide-render-path-completion.md`.
Side findings: the loader timeout-teardown segfault (exit 139) still suppresses
the graceful-exit per-ordinal summary, and the build tree is now
`build/win32_x86_debug`. No product code was changed.
