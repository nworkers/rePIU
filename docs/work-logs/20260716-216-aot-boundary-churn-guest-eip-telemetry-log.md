# AOT boundary/reentry churn 구간 실시간 게스트 EIP 계측 작업 로그
# Work Log: Live Guest-EIP Telemetry for the AOT Boundary/Reentry Churn Window

## 1. 개요 (Overview)

Task 215가 90초 재검증에서 "`0x030F6574` cross-segment thunk assertion storm 재발"로 결론지은
관측을 재검토했다. 설계(`docs/design/20260716-216-aot-boundary-churn-guest-eip-telemetry.md`)에서
지적한 대로 `last_eip`/`last_guest_eip`는 완전 dispatch에서만 갱신되는 stale 필드이므로, 동결
구간의 실제 원인을 확인하려면 경량 VEH 경로(`aot_boundary_count`/`aot_reentry_count`)가 다루는
게스트 주소를 실시간으로 노출해야 했다.

## 2. 구현 (Implementation)

1. `include/repiu/platform/win32/live_telemetry.h`: `aot_boundary_guest_eip`,
   `aot_legacy_fallback_count`, `aot_last_fallback_address` 필드 추가, 버전 11 → 12.
2. `src/platform/win32/execution_trampoline.cpp`의 `HandleAotReentry`:
   * 캐시 미스 경계 진입 시(`win32_context->Eip = guest_address` 직후) `aot_boundary_guest_eip`를
     `InterlockedExchange`로 라이브 미러링.
   * legacy fallback 진입 시 기존 `aot_legacy_fallback_count.fetch_add`/`aot_last_fallback_address.store`
     옆에 동일한 값을 `shared_live_telemetry`에도 `InterlockedIncrement`/`InterlockedExchange`로 미러링.
3. `src/host/win32/supervisor_main.cpp`의 `PrintSnapshot`에
   `aot_boundary_guest=0x.. legacy_fallback_count/addr=../0x..` 출력을 추가.
4. `repiu_loader_win32`/`repiu_supervisor_win32` Debug 재빌드 성공.

## 3. 검증 결과 (Verification Results)

`REPIU_EXECUTION_BACKEND=aot-dynamic`으로 `pumpit1`을 50초 재구동했다(`task216-verify-50s.log`).

| elapsed_ms | dispatch | aot_boundary/reentry | aot_boundary_guest_eip | legacy_fallback_count |
|---|---|---|---|---|
| 21625 (동결 시작) | 56859/56859 | 50650/50685 | `0x030EE1DA` | 0 |
| 30907 | 56859/56859 | 56225/56260 | `0x030EE1DA` | 0 |
| 41157 | 56859/56859 | 62370/62405 | `0x030EE1DA` | 0 |
| 50125 (종료) | 56859/56859 | 67732/67767 | `0x030EE1DA` | 0 |

`dispatch`가 `56859/56859`로 동결된 21.6초 지점부터 50초 종료까지 `aot_boundary_guest_eip`는 **단
한 번도 변하지 않고 `0x030EE1DA` 한 주소에 고정**되었다. `aot_boundary/reentry`는 이 구간 동안
50650/50685 → 67732/67767로 증가(총 17,082회, 약 28.5초 동안 초당 약 599회)했지만
`legacy_fallback_count`는 0을 유지해 완전 legacy(무기한 단일 스텝) 모드로는 전이하지 않았다.

`repiu_aot_probe.exe build/runtime_mounts/pumpit1/PIU/PIU.EXE 0x010EE1DA`(aot_probe 기준 주소,
런타임 주소 `0x030EE1DA`의 `-0x02000000`)로 정적 역어셈블한 결과:

```asm
0x030EE1CC  mov [0x033A6190], ebx
0x030EE1D2  add esp, 0x04
0x030EE1D5  pop ebp
0x030EE1D6  pop edi
0x030EE1D7  pop esi
0x030EE1D8  pop ecx
0x030EE1D9  pop ebx
0x030EE1DA  ret          ; aot_boundary_guest_eip 고정 지점, kind=4(RET)
```

정적 AOT 캐시 플랜의 `cache.address_map`에는 이 주소에 대한 반환 전용 디스패치 thunk 엔트리가
이미 존재한다(`query_cache=0x10ee1da,offset=0x11c52,guest_length=1,emitted_length=27,
bytes=9c817c240400000000e90b000000909d8d642404e9000000009dcc` — `pushfd`/비교/`jmp`/`popfd`/
`lea esp,[esp+4]`/패치형 `jmp rel32`/`int3` 형태의 표준 반환 디스패치 thunk 바이트 시퀀스).

## 4. 결론 (Conclusion)

Task 215의 "`0x030F6574` storm" 결론은 stale `last_guest_eip`에 근거한 오판정이었다(Task 205가
이미 한 번 지적한 것과 동일한 함정). 실제 동결 구간의 정체는 **guest `0x030EE1DA`(RET)가 정적
계획상 존재하는 전용 반환 디스패치 thunk를 타지 못하고, 매번 범용 `HandleAotReentry` 인라인
캐시 미스 경계(단일 스텝 1회 + 재탐색)로만 반복 진입**하는 것이다. `aot_return_dispatch_count`가
이 구간에서 전혀 증가하지 않는다는 사실(Task 215 로그의 동일 필드 확인)과 결합하면, 이 RET가
속한 코드 페이지의 캐시 엔트리가 self-modifying-code write-watch 등으로 반복 retire/재해석되고
있을 가능성이 유력하다(`docs/work-orders/20260712-191-aot-self-modifying-page-coherence.md`,
`docs/analysis/aot-self-modifying-code.md`와 같은 계열).

`docs/analysis/current-execution-frontier.md`에 Task 216 항목을 추가해 Task 215의 결론을
정정했다.

## 5. 다음 단계 (Next Steps)

`aot_retired_entry_trap_count`/`aot_quarantine_count`/`aot_page_retire_success_count`를 같은
방식으로 라이브 텔레메트리에 미러링해, 이 페이지가 실제로 retire/재해석을 반복하는지 실시간으로
확정해야 한다. 확정되면 (a) 해당 페이지의 self-modifying-code 오탐 조건 수정, 또는 (b)
`HandleAotReentry`가 캐시 미스 시 전용 반환 thunk를 먼저 재시도하도록 순서를 조정하는 것이 다음
구현 후보다.

## 6. 참고 (References)

* 로그: `task216-verify-50s.log`(세션 스크래치패드, UTF-16LE → `task216-verify-50s.utf8.txt`)
* 정적 역어셈블 출력: `probe_30ee1da.txt`(세션 스크래치패드)
* 관련 문서: `docs/design/20260716-216-aot-boundary-churn-guest-eip-telemetry.md`,
  `docs/analysis/current-execution-frontier.md`(Task 215, Task 216 항목)
