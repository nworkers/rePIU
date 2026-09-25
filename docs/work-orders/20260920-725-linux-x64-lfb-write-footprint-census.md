# Task 725 작업 지시 — Linux x64 LFB write footprint census

설계: [20260920-725](../design/20260920-725-linux-x64-lfb-write-footprint-census.md)

## 범위

1. 공용 LFB footprint census profile과 snapshot, 환경 정책을 추가합니다.
2. write lock 성공 뒤 baseline을 저장하고 unlock 전에 변경 footprint를 집계합니다.
3. final report와 결정적 probe를 연결합니다.
4. Linux x64/Win32 x86 build·core probe 및 Linux bounded observation을 실행합니다.
5. architecture, analysis, 작업 로그에 관찰값과 byte-difference 한계를 기록합니다.

## 제외

이번 작업은 readback/encode/present 동작을 최적화하거나, write 영역을 추정하여 기존 픽셀을
재사용하지 않습니다. guest 메모리 보호나 dirty-page 추적도 도입하지 않습니다.

---

# English

Design: [20260920-725](../design/20260920-725-linux-x64-lfb-write-footprint-census.md)

## Scope

Add a shared LFB footprint census profile, snapshot, and environment policy; save a baseline
after a successful write lock and aggregate its changed footprint before unlock; connect the
final report and deterministic probe; run Linux x64/Win32 x86 build/core probes and bounded
Linux observation; document observations and the byte-difference limit.

## Exclusions

This task does not optimize readback, encoding, or presentation; infer a write region to
reuse existing pixels; or introduce guest-memory protection or dirty-page tracking.
