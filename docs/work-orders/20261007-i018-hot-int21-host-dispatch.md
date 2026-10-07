# 작업 지시: 뜨거운 INT 21h 서비스의 호스트 직접 디스패치 (issue #18)

설계: `docs/design/20261007-i018-hot-int21-host-dispatch.md`

1. `src/engine/aot/aot_dbt_hle_dispatch.cpp`
   * `RequiresVehMediatedHle`에 EAX 매개변수를 더하고, `INT 0x21` +
     AH ∈ {0x2C, 0x3F, 0x42}를 허용 예외로 둔다. 호출부는 frame의
     EAX(frame[7])를 넘긴다.
2. 검증
   * probe superblock 절 통과, pumpitea 90초 toggle on/off 교대 ≥2쌍
     (두 공백, dispatch 카운터), 3종 게임 toggle-on 스모크.
3. 결과를 분석 문서와 issue #18에 기록한다. 승격(기본 켜기)은 측정
   결과를 보고 별도 판단한다.

---

# Work order: host-direct dispatch for hot INT 21h services (issue #18)

(1) Extend `RequiresVehMediatedHle` with an EAX parameter and allow
`INT 0x21` with AH in {0x2C, 0x3F, 0x42}, the caller passing frame[7].
(2) Verify: probe superblock section, interleaved pumpitea toggle
on/off pairs (both gaps, dispatch counters), three-game smoke with the
toggle on. (3) Record results in the analysis topic and issue #18;
default-on promotion is a separate decision.
