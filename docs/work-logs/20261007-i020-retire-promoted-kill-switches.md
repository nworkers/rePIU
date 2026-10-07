# 작업 로그: 오래 승격된 기능의 끄기 스위치 제거 (issue #20)

작업 지시: `docs/work-orders/20261007-i020-retire-promoted-kill-switches.md`
설계: `docs/design/20261007-i020-retire-promoted-kill-switches.md`
조사: `docs/analysis/environment-toggle-inventory.md`

## 한 일

1. **조사·기록**: 코드가 읽는 `REPIU_*` 191개 중 기능 토글 약 45개를 분류해
   `docs/analysis/environment-toggle-inventory.md`에 누적했다. 2번 묶음(버려진 옵트인
   실험)과 `REPIU_AOT_GUARDED_SEGMENT_LOAD/POP/READ` 보류는 재개 조건과 함께
   `docs/TODO.md`에 올렸다. (조사 직후 사용자에게 1번 묶음을 16개로 보고했으나 다시 세어
   15개다.)
2. **코드** — 15개 변수를 더 이상 읽지 않는다.
   * loader: AOT 빌드 토글 5개(`PORT_IO`·`RETURN_MISS`·`DIRECT_EDGE` 디스패치, timer
     safe point, direct return table)를 `use_dynamic_backend`로. 빌드 옵션 필드는
     legacy 백엔드와 probe 대조군 때문에 남겼다.
   * inline cache 패치: 스위치와 worker 왕복 경로(worker의 `kPatchInlineCache` 처리,
     요청용 원자 변수 3개, `aot_inline_cache_worker_patch_count`)를 제거. 요약 줄은
     `AOT inline cache patches: N`으로 바뀌었다.
   * Glide: setter 생략 스위치 4개와 판정 함수(네 gate 목록 무조건 적용, 요약 줄에서
     묶음 플래그 3개 제거), draw batch 스위치, host wait 스위치, gate pump 스위치와 옛
     `PumpEvents()` 호출.
   * port I/O delay loop 스위치, native span reject cache 설정(기존 기본값으로 고정 —
     legacy에서 `=1`로 켜던 수단만 사라짐).
3. **probe**: setter cache·draw batch probe의 정책 단언 제거, native span probe의 reject
   cache 단언을 "동적=켜짐, legacy=꺼짐"으로. 단독 실행 플래그 `--inline-cache`,
   `--native-linear-span`, `--glide-draw-batch`, `--glide-setter-state-cache`를 더했다
   (전체 체인은 main에서도 앞단 `dbt_indirect_dispatch`에서 멈춰 이 probe들에 닿지 못함).
4. **문서·스크립트**: README(항목 7개를 "항상 켜진 최적화" 한 항목으로), ARCHITECTURE
   6곳, 가이드 3종(종료된 A/B로 표시), 과거 A/B 스크립트 2종에 머리 주석.

## 검증

* Win32 x86 Release `repiu`·`repiu_aot_probe` 빌드(타깃별로 따로 빌드해 산출물 시각 확인).
* probe 6종 통과: `--inline-cache`, `--native-linear-span`, `--glide-draw-batch`,
  `--glide-setter-state-cache`, `--selector-guard`, `--segment-restore`.
* **16개 롬셋 30초 스모크, 기준(v0.0.206 빌드)과 새 빌드를 게임마다 교대**: 예외는 양쪽
  모두 0. 프레임 줄 수는 15개 롬셋에서 ±3 이내로 같았다. pumpitp2만 새 빌드 첫 실행이
  프레임 0(예외 없음, 창은 열리고 게스트는 `0x0402B720` 부근 대기 루프)이었으나 교대
  재검에서 새 빌드 3회 모두 정상(12·18·19), 기준 2회 정상(17·18) — 간헐이며, 이 롬셋은
  v0.0.206 승격 검증 때도 무프레임이 한 번 나왔다.
* pumpitea 90초 한 쌍: 로고 다음 공백 기준 2.78초 / 새 2.79초, 두 번째 정지 1.55초 /
  2.69초(이 축의 런 간 분산 범위 1.4~3.1초 안), 예외 0. port I/O delay loop 통계(시도
  98.5k/98.8k, batch 20.4k/20.5k)가 같아 일괄 처리가 그대로 동작함을 확인했다.
* Linux 빌드는 이 환경에서 확인하지 않았다. 바뀐 파일은 플랫폼 공용 엔진 코드이고 Linux
  전용 코드는 건드리지 않았다.

---

# Work log: retire the kill switches of long-promoted features (issue #20)

**Done.** Surveyed the 191 `REPIU_*` variables (about 45 feature toggles) into
`docs/analysis/environment-toggle-inventory.md` and recorded group 2 and the
`REPIU_AOT_GUARDED_SEGMENT_*` hold in `docs/TODO.md` with resume conditions (group 1 was first
reported to the user as 16; it is 15). Code no longer reads the 15 variables: the five AOT build
toggles follow `use_dynamic_backend` (fields kept for the legacy backend and probe controls);
the inline-cache toggle goes with the worker round-trip path and its counter (summary line now
`AOT inline cache patches: N`); the four setter-elision switches, draw batch, host wait and
gate pump (with the old `PumpEvents()` call) go; the delay-loop switch goes; the reject cache
is fixed at its previous default (legacy loses its `=1` opt-in). Probes were updated and gained
standalone flags (`--inline-cache`, `--native-linear-span`, `--glide-draw-batch`,
`--glide-setter-state-cache`) because the full chain stops earlier on main. README,
ARCHITECTURE, three guides and two historical A/B scripts were updated.

**Verified.** Win32 x86 Release builds; six probes pass; a 16-romset 30 s smoke alternating the
v0.0.206 build and the new build per game shows zero exceptions on both sides and matching frame
counts on 15 romsets, with pumpitp2's single zero-frame new-build run clearing on an interleaved
retest (new 12/18/19, base 17/18 — intermittent, also seen once on the baseline during the
v0.0.206 promotion); a 90 s pumpitea pair keeps the post-logo gap (2.78 s base / 2.79 s new),
the second stall within its run-to-run range, zero exceptions and identical delay-loop
statistics. Linux was not built here; only platform-neutral engine code changed.
