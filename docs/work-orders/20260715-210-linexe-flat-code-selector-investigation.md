# 2026-07-15 LINEXE flat 코드 selector 검증 및 thunk assertion 해소 작업 지시서
# 2026-07-15 LINEXE Flat Code Selector Verification and Thunk Assertion Resolution Work Order

## 1. 목적 (Objective)

`docs/analysis/20260715-209-aot-dynamic-import-stub-storm.md`에서 확정한 실제 frontier —
DOS4GW 런타임의 cross-segment call thunk 패처가 `cmp dx, cx` assertion에서 무한 재시도하는 문제 —
를 해소합니다. 유력 가설인 "LINEXE 내부 모듈 별도 selector vs 실기 DOS4GW flat 코드 세그먼트 공유"의
사실 여부를 확정하고, 확정 결과에 따라 selector 통합 또는 resolver의 cross-segment 의미 재현을
구현합니다.

Resolve the actual frontier established in
`docs/analysis/20260715-209-aot-dynamic-import-stub-storm.md`: the DOS4GW runtime's cross-segment call
thunk patcher endlessly retrying its `cmp dx, cx` assertion. Confirm or refute the leading hypothesis
(our per-module LINEXE selectors vs. real DOS4GW's flat shared code segment), then implement either
selector unification or faithful cross-segment resolver semantics accordingly.

## 2. 작업 내용 (Tasks)

1. **`0x010F3648` 해석 서브루틴 역추적** (aot_probe 주소 기준; 런타임은 `+0x02000000`)
   * `pop edx`로 반환되는 "대상 함수 세그먼트" 값이 어디서 계산되는지 정적 분석으로 확정합니다.
   * 내부의 DPMI `int 31h AX=0007`(Set Segment Base) 호출 2회가 각각 어떤 selector
     (`[0x013D68C6]`, `[0x013D68C4]`)를 조작하는지, 그 selector들이 로더의 어느 등록 경로에서
     오는지 추적합니다.
2. **selector 모델 검증**
   * 실기 DOS4GW/DOS16M 문서 및 기존 분석(`docs/analysis/dos4gw-loader-selector-allocation.md`,
     `docs/analysis/piu-linexe-call-gate-abi.md`)과 대조하여, LINEXE_LOADER 내부 모듈이 PIU.EXE
     본체와 CS를 공유해야 하는지 확정합니다.
   * 필요 시 trap 백엔드에서 `0x030F3420`(cmp 지점) 도달 시의 `dx`/`cx` 실측값을 기록하는 임시 진단을
     추가해 가설을 실증합니다.
3. **확정 결과에 따른 수정 구현**
   * (가설이 맞으면) LINEXE 코드 selector를 PIU.EXE 본체 CS와 통합하거나, resolver가 반환하는
     세그먼트 값을 현재 CS와 일치시키는 등가 수정을 구현합니다.
   * (가설이 틀리면) `dx` 계산 경로에서 발견된 실제 결함을 수정합니다.
4. **aot-dynamic 백엔드 회귀 재검**
   * 수정 후 `0x030F3438` 재시도 폭풍이 소멸하는지, `progress` 카운터가 진행하는지 확인합니다.

## 3. 검증 계획 (Verification Plan)

1. `scripts/build_win32_x86.ps1` 빌드 성공.
2. 기본(trap) 백엔드 30초: `diagnostic progress count`가 기존(~112k) 수준 이상, 회귀 없음.
3. `REPIU_EXECUTION_BACKEND=aot-dynamic` 30초:
   * `exception dispatch last EIP`가 `0x030F3438`에 고정되지 않음
   * `diagnostic progress count > 0`
   * (도달 시) `LINEXE bridge entry > 0`, `Glide gate entries > 0`
4. 두 백엔드 모두 크래시(비정상 exit code) 없음.

## 4. 참고 (References)

* `docs/analysis/20260715-209-aot-dynamic-import-stub-storm.md` — 근본 원인 분석과 미확정 목록
* `docs/analysis/current-execution-frontier.md` 2026-07-15 (Task 208–209) 항목
* `docs/work-logs/20260715-206-exception-diagnostic-and-buffer-provenance-log.md` — 재현 실패로
  정정된 이전 결론 (POP 개입 제거로 해소했다는 판정)
* 진단 시 주의: LINEXE 진단 카운터 대부분은 trap 백엔드 전용(`enable_single_step_trace` 게이트).
  aot-dynamic 결과만으로 판단하지 말 것.
