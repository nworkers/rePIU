# 20260723-275 작업 로그: 네이티브 직선 span 실행

## 한국어

### 구현

함수형 native region이 포착하지 못하는 일반 single-step EIP에서 다음 경계까지의 직선
명령을 실행하는 `native_linear_span.*`을 추가했습니다. scanner는 Zydis로 HLE 민감
명령, 모든 control transfer, 명시적 memory write를 경계로 판별합니다. 경계 앞에 두
명령 이상이 있을 때 Dr0을 경계에 두고 TF를 끕니다.

경계 #DB 또는 예상하지 않은 exception에서는 Dr0/Dr6/Dr7과 TF를 복원하고 기존 handler
chain으로 돌아갑니다. 게스트 byte를 수정하지 않으며 memory write를 span에 포함하지 않고
scan 결과도 cache하지 않습니다. `REPIU_NATIVE_LINEAR_SPAN=1` opt-in이며 기본 동작은
변하지 않습니다.

### 검증

- Win32 x86 Debug 전체 빌드 성공
- synthetic probe 통과:
  - `linear_span_control_boundary=true`
  - `linear_span_sensitive_boundary=true`
  - `linear_span_write_boundary=true`
  - `linear_span_short_rejected=true`
  - `linear_span_all=true`
- 기존 inline-cache 및 SMC coherence probe 전체 통과
- 5초 smoke A/B 통과
- 30초 기본 `aot-dynamic` A/B 통과
- 240초 기본 `aot-dynamic` A/B 통과
- 모든 ON 실행에서 span cancel 0, fatal 0, legacy fallback 0
- 최종 EEPROM 사본과 원본 SHA-256 일치

최종 원시 결과:
`build/benchmarks/native-linear-span/20260723-134050/results.csv`

| 지표 | OFF | ON | 변화 |
|---|---:|---:|---:|
| single-step | 1,356,719 | 947,256 | -30.2% |
| span native 명령 | 0 | 953,580 | +953,580 |
| guest 명령 proxy | 1,356,719 | 1,900,836 | +40.1% |
| 기존 progress | 49,580 | 59,059 | +19.1% |
| window open | 14.406초 | 14.281초 | 사실상 동일 |
| span entry/boundary/cancel | 0/0/0 | 217,747/217,747/0 | 안전 종료 |
| fatal / fallback | 0 / 0 | 0 / 0 | 무회귀 |

240초 두 실행 모두 texture와 swap에는 도달하지 않았습니다. 따라서 내부 coverage 개선은
확인됐지만 19.1% progress 차이를 사용자 체감 wall-clock 향상으로 해석하지 않습니다.
기능은 기본 활성화하지 않고 opt-in으로 유지합니다.

### 비교 조건 보정

초기 120초 × 4 및 첫 240초 × 2 측정은 기존 실험용
`REPIU_NATIVE_REGION=1`을 공통으로 켠 함수형 region 기준이었습니다. 이 조건에서도 span
cancel/fatal은 0이고 명령 proxy가 증가했지만, 현재 제품 기본 경로 비교가 아니므로 최종
성능 표에서는 제외했습니다. 구현을 함수형 region과 독립시킨 뒤
`REPIU_NATIVE_REGION`을 제거한 기본 clean-function fast path로 30초와 240초 A/B를 다시
측정했습니다.

## English

### Implementation

Added `native_linear_span.*` to run straight-line instructions from an ordinary single-step
EIP to the next boundary that function-level native regions cannot cover. Zydis treats
HLE-sensitive instructions, all control transfers, and explicit memory writes as boundaries.
With at least two preceding instructions, Dr0 guards the boundary while TF is clear.

The boundary #DB or any unexpected exception restores Dr0/Dr6/Dr7 and TF, then falls through
to the existing handler chain. No guest byte is modified, memory writes remain outside
spans, and scan results are not cached. `REPIU_NATIVE_LINEAR_SPAN=1` is opt-in and the
default path is unchanged.

### Verification and result

The full Win32 x86 Debug build passed. Synthetic control, sensitive, write, and short-span
checks passed, as did all existing inline-cache and SMC coherence probes. Smoke, 30-second,
and 240-second same-binary A/B runs completed. Every enabled run had zero span cancellation,
fatal, and legacy fallback, and EEPROM hashes matched.

In the final production-policy 240-second pair, single steps fell 30.2% and the local guest
instruction proxy rose 40.1%. All 217,747 spans reached their boundary. Existing progress
rose 19.1%, but window-open time was unchanged and neither run reached texture or swap.
The progress difference is therefore not claimed as user-visible wall-clock speedup. The
feature remains opt-in.

### Corrected comparison condition

Preliminary 120-second × 4 and the first 240-second × 2 measurements shared the experimental
`REPIU_NATIVE_REGION=1` function-region policy. They also showed zero cancellation/fatal and
higher instruction proxy, but were excluded from the final table because that is not the
production default. The span path was made independent and remeasured with the default
clean-function fast path and `REPIU_NATIVE_REGION` removed.
