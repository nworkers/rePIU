# 20260724-286 작업 로그: AOT-DBT dispatch-site 수명 안전

## 한국어

### 구현

- indirect와 RET host adapter의 `FindDispatchSite`를 pointer 반환에서
  `bool + caller-owned out value`로 변경했습니다.
- resolver 진입 전에 dispatch site 전체를 local value로 복사하고, resolver 전후
  fallback/success continuation 계산이 snapshot만 사용하도록 통일했습니다.
- resolver를 가로질러 placement의 dispatch-site vector 원소 포인터나 참조를
  보존하는 다른 AOT adapter 패턴이 없음을 검색했습니다.
- `scripts/task286_dispatch_site_lifetime_ab.ps1`를 추가해 calls-only와 indirect-off
  조건을 직접 loader, 격리 EEPROM, 고정 timeout으로 재현하도록 했습니다.

### 정적·빌드 검증

- `site->`, vector 원소 주소 반환, `const ...Site*` 패턴이 Win32 AOT adapter에
  남지 않았음을 확인했습니다.
- Visual Studio Win32 x86 Debug 전체 빌드가 성공했습니다.
- 주요 probe가 모두 통과했습니다:
  - `dbt_return_fallback_all=true`
  - `dbt_indirect_dispatch_all=true`
  - `dbt_call_return_trace=true`
  - `dbt_call_step_probe=true`
  - `coherence_all=true`
- 기존 C4819와 Zydis LNK4217 경고 외 새 오류는 없습니다.

### sequence 56 재검증

결과 디렉터리:
`build/task285-call-step-seq56-20260724-174534/`

- exception 없음, 90초 timeout
- step summary `1/3/1/1/0/0/idle`
- CALL `0x030D913B -> 0x03086094`, guest return `0x030D913E`
- pre-C3: EIP `0x0D768C89`, ESP `0x035D68C0`, match
- stack: cache target `0x0D84213A`, guest return `0x030D913E`
- post-C3: EIP `0x0D84213A`, ESP `0x035D68C4`, match
- return: EIP `0x0D768B8B`, ESP `0x035D68C8`, match
- poison continuation과 크래시 없음

### 240초 실구동 A/B

| 조건 | 결과 디렉터리 | 예외 | progress | indirect `entry/attempt/success/fallback` |
|---|---|---|---:|---:|
| calls-only | `build/task286-dispatch-lifetime-call-20260724-174912/` | 없음, timeout | 95,842 | `33741/33741/60/33681` |
| indirect off | `build/task286-dispatch-lifetime-0-20260724-175442/` | 없음, timeout | 94,836 | `0/0/0/0` |

calls-only는 texture download 2회, buffer clear 2회, buffer swap 1회까지 도달했고,
control은 texture download 2회와 buffer clear 1회까지 도달했습니다. 두 실행 모두
창을 640x480으로 열었고 EEPROM SHA-256은 fixture와 같은
`A1FC1D120EF12DE4FB3608551750F93E02F911F26A3DDF9054ABCE4846652570`입니다.

수정 전 30~50초에 재현되던 Glide access violation은 calls-only 240초 동안
재현되지 않았습니다. 이로써 use-after-reallocation 수정의 기능·안전성 목표를
충족했습니다.

### 결론

Task 282 CALL 경로 크래시의 근인은 해결됐습니다. 그러나 calls-only의 실제 성공은
33,741회 시도 중 60회(약 0.18%)뿐입니다. progress는 control보다 약 1.1% 높았지만
단일 실행 timing 차이로부터 분리할 수 없으므로 성능 이득의 증거로 사용하지 않습니다.
CALL host dispatch는 계속 opt-in으로 유지하며, 기본 활성화는 반복 성능 측정 또는
fallback/quarantine 개선을 별도 설계한 뒤 재검토합니다.

## English

Task 286 changes both indirect and RET host adapters from placement-vector element pointers
to caller-owned site snapshots. Each complete site is copied before resolver entry, and all
fallback/success continuation calculations use only the local value. A source audit found
no remaining Win32 AOT adapter pattern that retains a dispatch-site pointer or reference
across a resolver. The task also adds a direct-loader, isolated-EEPROM A/B script.

The full Visual Studio Win32 x86 Debug build passed. Return fallback, indirect dispatch,
CALL/RET trace, CALL step, and coherence probes all passed; only the pre-existing C4819 and
Zydis LNK4217 warnings remain.

The repeated sequence 56 run captured matching pre-C3, post-C3, and return-target EIP/ESP.
Its stack held the valid cache target and guest return, with no poison continuation or
exception. In the probe-off 240-second A/B, calls-only completed without the former AV at
progress 95,842 and indirect accounting `33741/33741/60/33681`; indirect-off completed at
progress 94,836 with no indirect entries. Calls-only reached two texture downloads, two
buffer clears, and one buffer swap. Both EEPROM hashes matched the fixture.

The Task 282 CALL-path crash root cause is therefore fixed. CALL host dispatch remains
opt-in because only 60 of 33,741 attempts succeeded (about 0.18%), and the roughly 1.1%
single-run progress difference is not evidence of a performance improvement. Default
enablement requires separate repeated performance validation or a meaningful
fallback/quarantine reduction.
