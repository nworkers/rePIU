# 20260801-392 작업 지시: Hybrid Segment-Override Dispatch / Work Order

설계: [20260801-392-hybrid-segment-override-dispatch.md](../design/20260801-392-hybrid-segment-override-dispatch.md)

## 한국어

1. segment-override site에 동반 HLE dispatch slot offset을 추가합니다.
2. emitter가 native selector-guard slot과 HLE dispatch slot을 함께 생성하도록 변경합니다.
3. live re-resolution에서 NativeFolded/HleLowMemory/unresolved를 각각 native/HLE/INT3로 라우팅합니다.
4. dynamic append offset과 coverage validator를 hybrid layout에 맞춥니다.
5. synthetic probe로 native/HLE/unresolved patch와 누락 fallback을 검증합니다.
6. Release build, 두 PIU probe, 3초 A/B를 수행합니다.
7. 결과와 장시간 재검증 절차를 문서화하고 커밋합니다.

## English

1. Add the companion HLE-dispatch slot offset to segment-override sites.
2. Emit both the native selector-guard slot and HLE-dispatch slot.
3. Route NativeFolded/HleLowMemory/unresolved to native/HLE/INT3 during live re-resolution.
4. Update dynamic-append offsets and coverage validation for the hybrid layout.
5. Probe native/HLE/unresolved patching and missing fallback rejection.
6. Run Release builds, both PIU probes, and a matched three-second A/B.
7. Document results and the long revalidation procedure, then commit.
