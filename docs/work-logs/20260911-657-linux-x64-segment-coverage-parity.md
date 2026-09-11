# 20260911-657 작업 로그: Linux x64 segment coverage predicate parity

설계: [20260911-657 설계](../design/20260911-657-linux-x64-segment-coverage-parity.md)
작업 지시: [20260911-657 작업 지시](../work-orders/20260911-657-linux-x64-segment-coverage-parity.md)

## 한국어

### 결과

Task 656에서 확인한 `0x010F44E6` (`66 36 89 07`, `MOV SS:[EDI],AX`)의
coverage 거절 원인을 validator와 emitter의 layout predicate 불일치로 확정하고
수정했습니다.

### 변경 사항

* `absolute_disp32` 판정을 `mod=00` 전체에서 `mod=00 && rm=5`로 좁혀 emitter와
  일치시켰습니다. `rm=7` EDI-base 형식은 base register를 유지합니다.
* `disp.size=0`인 명령의 suffix 시작점을 `modrm.offset + 1`로 수정했습니다.
  displacement가 있는 형식은 기존처럼 displacement 끝에서 시작합니다.
* `long_mode_emission_probe`에 EDI-base segment override plan을 추가했습니다.
  정상 emitted layout과 coverage 통과, access byte corruption 거절을 함께
  검증합니다.
* 임시 validator 상세 진단 출력은 최종 코드에서 제거했습니다.

### 검증

Linux x64 Debug 빌드가 성공했고, core probe는 `27/27` 통과했습니다. 실제
`pumpit2a` bounded run에서는 `0x010F44E6` slot이 반복 생성되었으며 coverage 또는
translation failure가 발생하지 않았습니다. 실행은 이후 hot loop에 진입했고,
12초 bounded test의 timeout으로 종료했습니다. 해당 종료는 테스트 제어이며
프로그램 crash로 해석하지 않습니다.

이번 작업으로 `0x010F44E6` coverage frontier는 해소되었습니다. coverage 이후
정상적인 semantic progress를 막는 다음 frontier의 정확한 지점은 아직 확인되지
않았으므로, 다음 작업에서 별도 trace와 실행 상태 분석이 필요합니다.

## English

### Result

The coverage rejection at `0x010F44E6` (`66 36 89 07`, `MOV SS:[EDI],AX`) from
Task 656 was confirmed to be a layout-predicate mismatch between the validator and
the emitter, and was fixed.

### Changes

* Changed `absolute_disp32` from all `mod=00` forms to `mod=00 && rm=5` to match the
  emitter. The `rm=7` EDI-base form preserves its base register.
* Changed the suffix start for `disp.size=0` instructions to `modrm.offset + 1`.
  Forms with a displacement still start after the displacement.
* Added an EDI-base segment-override plan to `long_mode_emission_probe`, covering
  normal emitted layout, successful coverage validation, and rejection after access
  byte corruption.
* Removed the temporary detailed validator diagnostic output from the final code.

### Verification

The Linux x64 Debug build succeeded, and the core probe passed `27/27`. In a bounded
real `pumpit2a` run, the `0x010F44E6` slot was emitted repeatedly with no coverage or
translation failure. Execution then entered a hot loop and the 12-second bounded test
was ended by its timeout. This was controlled test termination and is not interpreted
as a program crash.

The `0x010F44E6` coverage frontier is cleared. The exact next frontier preventing
normal semantic progress after coverage is not yet identified and requires a separate
trace and execution-state analysis task.
