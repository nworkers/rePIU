# Task 725 작업 로그 — Linux x64 LFB write footprint census

설계: [20260920-725](../design/20260920-725-linux-x64-lfb-write-footprint-census.md)
작업 지시: [20260920-725](../work-orders/20260920-725-linux-x64-lfb-write-footprint-census.md)
frontier: [linux-port-frontier](../analysis/linux-port-frontier.md)
선행: [20260919-724](20260919-724-linux-x64-lfb-lock-phase-timing.md)

## 결과

`GlideLfbWriteFootprintProfile`을 추가했습니다. `REPIU_GLIDE_LFB_WRITE_CENSUS=1|on|true`일 때만
성공한 write `grLfbLock`의 seeded RGB565 staging bytes를 private baseline에 보관하고 대응 unlock에서
pixel 단위 difference를 집계합니다. off 경로는 baseline의 할당·복사·비교를 수행하지 않으며 guest
memory, `lfbPtr`, OpenGL decode/present 호출 순서, 반환값을 바꾸지 않습니다.

30초 Linux x64 `pumpit2a` bounded run: 304 lock 비교, unchanged 19, partial extent 285,
full extent 0, all-pixels-changed 0, malformed 0이었습니다. 누적 changed pixel은 21,659,082,
최대 changed pixel은 115,200, 최대 bounding box는 306,081 pixels였습니다. profile timing은
동시에 켜지지 않아 disabled summary를 유지했습니다.

## 검증

- Linux x64 Debug `repiu`, `repiu_core_probe` 재빌드 성공; core probe 30/30 통과
- Linux x64 census bounded run: timeout immediate-exit, `repiu-fault` 없음, 완결 summary 확인
- Win32 x86 Debug 전체 build 성공 (기존 C4819 encoding warnings만 발생)
- Win32 x86 `repiu_aot_probe --glide-lfb-write-footprint` 성공: policy, aggregation, malformed 모두 true
- Win32 x86 core probe는 전체 build 뒤 기존 `mode16_push_writes=false` group으로 28개 중 1개 실패.
  LFB footprint code의 결정적 probe가 아닌 별도 synthetic probe 문제로 기록합니다.

## 다음

partial byte footprint는 readback을 제거할 권한이 아닙니다. 동일값 재기록은 보이지 않고 최대
bounding box도 거의 전체 화면을 덮습니다. 후속 작업에서 guest store 또는 원본 call-site의 실제
write 범위와 pixel ownership을 보수적으로 증명해야 합니다.

---

# English

Design: [20260920-725](../design/20260920-725-linux-x64-lfb-write-footprint-census.md)
Work order: [20260920-725](../work-orders/20260920-725-linux-x64-lfb-write-footprint-census.md)
Frontier: [linux-port-frontier](../analysis/linux-port-frontier.md)
Predecessor: [20260919-724](20260919-724-linux-x64-lfb-lock-phase-timing.md)

## Result

Added `GlideLfbWriteFootprintProfile`. Only with `REPIU_GLIDE_LFB_WRITE_CENSUS=1|on|true`
does a successful write `grLfbLock` retain its seeded RGB565 staging bytes in a private
baseline and compare pixel differences at matching unlock. The off path neither allocates,
copies, nor compares a baseline, and it does not change guest memory, `lfbPtr`, OpenGL
decode/present order, or return values.

A 30-second Linux x64 bounded `pumpit2a` run compared 304 locks: unchanged 19, partial
extent 285, full extent 0, all pixels changed 0, malformed 0. It saw 21,659,082 total
changed pixels, 115,200 maximum changed pixels, and a 306,081-pixel maximum bounding box.
Timing stayed independently disabled.

## Verification

- Linux x64 Debug `repiu` and `repiu_core_probe` rebuilt; core probe passed 30/30.
- Linux x64 bounded census run reached timeout immediate-exit with no `repiu-fault` and a complete summary.
- Full Win32 x86 Debug build succeeded with only pre-existing C4819 encoding warnings.
- Windows x86 `repiu_aot_probe --glide-lfb-write-footprint` passed policy, aggregation, and malformed checks.
- After the full build, Win32 x86 core probe failed one of 28 groups: the existing
  `mode16_push_writes=false` group. This is recorded as a separate synthetic-probe issue,
  not the LFB footprint deterministic probe.

## Next

A partial byte footprint does not authorize removing readback. Identical rewrites are
invisible and a maximum bounding box nearly spans the screen. A follow-up must conservatively
prove actual guest-store ranges or original call-site pixel ownership.
