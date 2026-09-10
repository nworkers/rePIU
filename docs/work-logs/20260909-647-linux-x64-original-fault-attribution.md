# 작업 로그 647: Linux x64 원래 fault EIP attribution

## 결과

Linux x64에서 복구 목적지로 EIP가 덮어써진 뒤 발생하는 후속 fault를 원래
실행 위치에 연결할 수 있도록 bounded recovery provenance telemetry를
추가했습니다. 실행 경로와 복구 정책은 변경하지 않았습니다.

`ThreadContext::fault_recovery_provenance`는 `RecoverToHost` 직전에 source
EIP, source 영역(`guest`/`aot-cache`), 복구 경로(`fault-callback`/
`shutdown-interrupt`)를 저장합니다. `REPIU_FAULT_EXIT_TRACE=1`이면 기존
`[repiu-exit]` 출력에 `recovery_source_eip`, `recovery_source`,
`recovery_path`가 추가됩니다.

## 변경 사항

* `fault_recovery_provenance` 공용 타입·helper와 core probe를 추가했습니다.
* Linux fault callback 및 shutdown interrupt 복구 직전에 provenance를
  기록하도록 연결했습니다.
* telemetry 모듈, CMake source 목록, core probe 등록, `ARCHITECTURE.md`,
  Linux frontier 분석 문서를 갱신했습니다.

## 검증

* `git diff --check`: 통과했습니다. 기존 CRLF 파일의 줄바꿈 경고 외 오류는
  없었습니다.
* WSL이 복구된 뒤 `CMAKE_BUILD_PARALLEL_LEVEL=2`로 Linux x64 Debug headless
  `repiu_core_probe` 및 `repiu` 빌드를 성공했습니다.
* 새 core probe를 포함한 전체 결과는 `core_probe_total=26`,
  `core_probe_failures=0`, `core_probe_all=true`였습니다.
* 실제 `pumpit2a`에서 return resolver trace와 fault exit trace를 확인했습니다.
  `guest_source=0`, `producer=0x010F1E56`, `detail=dynamic AOT target is
  outside the guest arena`로 resolver가 zero를 반환하고, unresolved thunk의
  `INT3` `0x402BCF61` 뒤 `RecoverGuestStackException`의 `UD2` `0x402BCF62`로
  이어졌습니다. 이 경로에는 `RecoverToHost`가 없으므로
  `recovery_source_eip=0`/`none`은 기록 누락이 아니라 올바른 결과입니다.

## 다음 단계

다음 작업은 `0x010F1E56` RET가 zero return target을 만들게 된 원인과,
unresolved x64 return 경계에서 producer/target을 종료 trace에 연결하는
방법을 별도 설계하는 것입니다.

---

# Work Log 647: Linux x64 original fault EIP attribution

## Result

Added bounded recovery-provenance telemetry so a later fault after EIP has
been redirected to an x64 recovery destination can be attributed to its
original execution location. Execution and recovery policy were not changed.

`ThreadContext::fault_recovery_provenance` stores the source EIP, source region
(`guest`/`aot-cache`), and recovery path (`fault-callback`/
`shutdown-interrupt`) immediately before `RecoverToHost`. With
`REPIU_FAULT_EXIT_TRACE=1`, the existing `[repiu-exit]` output gains
`recovery_source_eip`, `recovery_source`, and `recovery_path`.

## Changes

* Added the shared `fault_recovery_provenance` type/helper and core probe.
* Connected provenance recording immediately before recovery from the Linux
  fault callback and shutdown interrupt paths.
* Updated the telemetry module, CMake source lists, core-probe registration,
  `ARCHITECTURE.md`, and the Linux frontier analysis.

## Verification

* `git diff --check`: passed. No errors were reported apart from line-ending
  warnings from existing CRLF files.
* After WSL recovered, the Linux x64 Debug headless `repiu_core_probe` and
  `repiu` builds succeeded with `CMAKE_BUILD_PARALLEL_LEVEL=2`.
* The full result including the new probe was `core_probe_total=26`,
  `core_probe_failures=0`, `core_probe_all=true`.
* A real `pumpit2a` run with return-resolver and fault-exit traces showed
  `guest_source=0`, `producer=0x010F1E56`, and
  `detail=dynamic AOT target is outside the guest arena`. The resolver returned
  zero, leading to the unresolved thunk `INT3` at `0x402BCF61` and then the
  `UD2` at `RecoverGuestStackException`, `0x402BCF62`. No `RecoverToHost` runs
  on this path, so `recovery_source_eip=0`/`none` is correct rather than a
  missing record.

## Next step

The next task should design how to identify why RET at `0x010F1E56` produces a
zero return target and how to connect producer/target data from the unresolved
x64 return boundary to the final exit trace.

---
