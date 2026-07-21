# 작업 로그 — Glide API 호출 감사와 R4 LFB 경로 구현 / Work Log — Glide API Call Audit and R4 LFB Path

* 작성일 / Date: 2026-07-21 (Task 257)
* 설계 / Design: `docs/design/20260721-257-glide-r4-lfb-path.md`
* 작업 지시 / Work order: `docs/work-orders/20260721-257-glide-r4-lfb-path.md`
* 브랜치 / Branch: `claude/glide-api-call-audit` (커밋 `63a067f`)

## 1. 배경 / Background

Glide HLE 전수 감사 요청에서 출발했다. 정적 검토로 세 등급(실동작 / 전용 no-op /
기본 핸들러)을 분류했으나, **어떤 API가 실제로 호출되는지**는 정적으로 알 수 없어
실행 관측이 필요했다.

## 2. 관측 수단의 문제와 해결 / Observation Gap and Fix

기존 계측만으로는 확정이 불가능했다.

* 게이트 진입 로그가 **96건 캡**에 걸려 콘텐츠 단계 이전에 소진된다.
* 종료 요약(`Win32 Glide call trace`, ordinal별 카운트)이 **타임아웃 경로에서
  출력되지 않는다**(design 249 §6이 지목한 "측정 선결 과제").

기존 코드에 이미 있던 ordinal 최초호출 감지 지점(`glide_call_counts[ordinal]++ == 0`)에
env-gated 로그 한 줄을 추가해 해결했다(`REPIU_GLIDE_CALL_AUDIT`). 출력이 **고유
export 수(≤97줄)로 상한**이 잡히므로 캡과 타임아웃을 동시에 우회하며, 각 API의 최초
호출 인자 7개도 함께 남는다.

## 3. 감사 결과 (확인됨) / Audit Result

`aot-dynamic pumpit1` 300초 구동. 카탈로그 97종 중 **39종 호출**, 거부 0건.
런타임이 스스로 `unhandled (default)`로 기록한 것은 **정확히 3개**.

| 등급 | 종수 | 내용 |
|---|---:|---|
| A. 실동작 | 29 | 초기화·상태·텍스처·클리어/스왑·`grDrawTriangle` |
| A-. 상태만 보존 | 2 | `grGlideInit`, `grLfbWriteColorFormat` |
| B. 전용 no-op | 5 | `grHints`, `grTexClampMode/FilterMode/MipMapMode`, `grTexCombine` |
| **C. 기본 핸들러(미처리)** | **3** | `grConstantColorValue`(92), `grLfbLock`(112), `grLfbUnlock`(113) |

**반증된 사전 추정.** 감사 전 "폴리곤/버텍스리스트 드로우 미구현이 BGA/UI 공백을
만들 가능성이 크다"고 추정했으나, **드로우는 `grDrawTriangle` 하나만 호출**되며
폴리곤·버텍스리스트·점·선·AA 계열 11종은 전부 미호출로 확인됐다. 드로우 계열 확장은
실질 우선순위가 아니다.

**design 249 R4 착수 조건 충족.** design 249는 LFB를 "실사용이 카운트로 확인될 때
착수"로 유보했는데, `grLfbLock`이 실제 호출되며 기본 핸들러가 **FXFALSE(lock 실패)**
를 돌려주고 있었다.

## 4. 구현 / Implementation

3계층으로 분리(AGENTS.md 거대 파일 금지 규칙).

| 파일 | 역할 |
|---|---|
| `include/repiu/hle/glide_lfb.h`, `src/hle/glide_lfb.cpp` (신규) | 스테이징 표면, `GrLfbInfo_t` 직렬화, 565↔RGBA8 변환 |
| `glide_opengl_backend.{h,cpp}` | `PresentLfbSurface`(업로드+전체화면 쿼드, GL 상태 격리), `ReadbackFramebuffer` |
| `linexe_glide_boundary.cpp` | `_GRLFBLOCK@24` / `_GRLFBUNLOCK@8` / `_GRCONSTANTCOLORVALUE@4` — ABI 해석·위임만 |

`GlideLogicalState`에 `constant_color` 추가, 상태 이미지 version 3으로 상향.

**설계 결정 — 스테이징 버퍼를 아레나가 아닌 호스트 할당으로.** 아레나 carve는
`BuildLinexeArenaLayout`을 고쳐 셀렉터·동적 할당자 경계 같은 전역 불변식을 건드려야
해 회귀 위험이 크다. 게스트는 Win32 flat DS로 네이티브 실행되므로 프로세스 내
커밋 주소면 충분하고, 이 포인터는 우리가 건네는 것이라 게스트 포인터 검증 대상이
아니다. 상세·전환 조건은 설계 §3.1.

**유지 정책 준수.** 미지원 조합(565 외 writeMode, FRONTBUFFER 등)은 `reject_gate`가
아니라 FXFALSE + 계측으로 정상 반환한다. Category A로 지적한 "미관측 인자 → reject →
게이트 예외 미인수 → 크래시" 패턴을 신규 코드에 재현하지 않기 위함이다.

## 5. 구현 중 발견·수정한 결함 2건 / Two Defects Found During Implementation

**(a) `size` 필드를 호출자 값으로 에코백.** `grLfbLock`은 출력 필드를 채우는 쪽이므로
`size`도 우리가 채운 레이아웃을 보고해야 한다. PIU는 이 필드를 0으로 넘긴다(표준은
호출자가 20을 채움). 값을 그대로 채워 넣도록 수정했다.

**(b) write lock이 기존 화면을 지움 — 실제 관측된 결함.** 실 하드웨어에서 LFB는
살아있는 프레임버퍼라, 일부 픽셀만 쓰면 나머지는 보존된다. 초기 구현은 0으로 채운
스테이징을 건네고 unlock에서 통째로 덮어써, **아무것도 안 쓴 lock이 전체 화면을
검게 칠했다**(직전 제출된 삼각형이 지워지는 것을 진단으로 확인: `nonblack 1 → 0`).
모든 lock에서 현재 백버퍼로 스테이징을 seeding하도록 수정했고, 함께
`ReadbackFramebuffer`가 `glReadPixels`의 bottom-up 행 순서를 소스에서 한 번
뒤집도록 했다(스테이징·텍스처 소비자는 모두 row 0 = 상단).

## 6. 검증 / Verification

* Win32 x86 Debug 빌드 성공.
* 게이트: `unhandled (default)` **3 → 0**, `gate rejected` **0 유지**.
* `GrLfbInfo_t` 왕복 확인 — unlock 시점 게스트 스택에 우리가 쓴 `size=0x14`,
  `lfbPtr`, `stride=0x500`이 순서대로 나타남.
* `size`를 offset 0에 20으로 써도 접근 위반 0건 → 게임이 offset 0을 포인터로
  역참조하지 않음(구조체 레이아웃이 어긋났을 가능성의 반증).

**미검증 (당시).** 게스트가 스테이징에 **0바이트**를 쓰고 정지해, 블릿의 실데이터
검증이 불가능했다. 이 원인은 Task 258에서 별건으로 규명됐다(§7).

## 7. 후속 / Follow-ups

* **Task 258 (해결됨):** 콘텐츠가 렌더되지 않던 근인은 LFB가 아니라 `GrLOD_t`
  해석 오류였다. `docs/work-logs/20260722-258-glide-lod-enumeration-fix-log.md` 참조.
* **LFB 실데이터 검증 미완:** Task 258 검증 구동에서는 게임이 입력에 따라 다른
  경로로 진입해 LFB를 호출하지 않았다. 실제 LFB 트래픽이 나오는 화면을 찾아
  블릿·seeding을 실데이터로 확인해야 한다.
* **순서 위험 (미확정):** 게임이 `unlock` 이후 `grBufferClear`를 호출하면 블릿이
  지워진다. 실 트래픽 관측 후 판단한다.
* **범위 밖 유지:** `grLfbWriteRegion`/`ReadRegion`, `ConstantAlpha/Depth`,
  `WriteColorSwizzle`, 565 외 writeMode, FRONTBUFFER lock, `pixelPipeline` 시맨틱.

---

## English Summary

A per-ordinal first-call audit (`REPIU_GLIDE_CALL_AUDIT`) settled which Glide
exports PIU actually reaches, working around both the 96-entry gate log cap and
the timeout path that skips the exit summary. Of 97 cataloged exports, 39 are
called and exactly three land on the default handler: `grConstantColorValue`,
`grLfbLock`, and `grLfbUnlock`. This disproved a prior inference — only
`grDrawTriangle` is used for drawing, so the polygon/vertex-list/AA families are
not a content gap — and satisfied design 249's condition for starting R4.

The LFB pair is implemented across a new platform-neutral `glide_lfb` module,
backend blit/readback entry points with explicit GL state isolation, and
delegating gate handlers. Two defects were found and fixed during implementation:
`grLfbLock` must fill `size` itself rather than echo the caller's value, and a
write lock must expose the live framebuffer — the initial zero-filled staging
buffer made unlock blit black over already-drawn content, observed erasing the
triangles submitted just before the lock.

Verified: unhandled gates 3 → 0, rejected gates still 0, and the `GrLfbInfo_t`
round-trip is visible in guest memory. Not verified: the guest wrote zero bytes,
so the blit had no real source data — the reason was traced in Task 258 to a
`GrLOD_t` misinterpretation, not to the LFB path.
