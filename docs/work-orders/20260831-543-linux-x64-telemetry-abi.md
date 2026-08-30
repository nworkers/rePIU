# 20260831-543 Linux x64 telemetry ABI 정규화 작업 지시서

## 한국어

### 목적

Linux x64에서 `long`의 폭이 8바이트로 바뀌어 실패하는 고정 shared telemetry ABI를
명시적인 32비트 field type으로 정규화하고, x64 compile probe를 다음 구조적 장벽까지
진행합니다.

### 작업 범위

- `SharedLiveTelemetry` field type을 고정 폭 32비트로 변경합니다.
- shared field와 32-bit placement counter에 필요한 atomic overload를 추가합니다.
- Windows supervisor 및 Linux pointer call site를 수정합니다.
- AOT 실행 모델, guest instruction, signal context, stack bridge는 변경하지 않습니다.

### 검증 기준

- Linux x64 compile probe가 `live_telemetry.h`의 `sizeof(long)` assertion을 통과합니다.
- shared telemetry field에 대한 atomic access가 4바이트로 유지됩니다.
- Linux i386 빌드의 기존 layout과 call site가 유지됩니다.
- 변경 후 첫 번째 x64 project-owned 오류를 작업 로그와 `linux-port-frontier.md`에
  기록합니다.

## 결과

고정 폭 telemetry 변경은 완료되었습니다. x64 probe는 다음 실행 계층 오류까지
진행했고, Linux i386 및 Win32 회귀 target은 통과했습니다. 다음 작업 단위는
x86-64 guest-entry 및 stack-bridge 설계입니다.

## English

### Objective

Normalize the fixed shared telemetry ABI that fails on Linux x64 because `long` becomes
eight bytes, then advance the x64 compile probe to the next structural barrier.

### Scope

- Change `SharedLiveTelemetry` fields to an explicit fixed-width 32-bit type.
- Add the atomic overloads required by shared fields and 32-bit placement counters.
- Update the Windows supervisor and Linux pointer call sites.
- Do not change the AOT execution model, guest instructions, signal context, or stack bridge.

### Verification criteria

- The Linux x64 compile probe passes the `sizeof(long)` assertion in `live_telemetry.h`.
- Atomic access to shared telemetry fields remains four bytes.
- Existing Linux i386 layout and call sites remain valid.
- Record the next project-owned x64 error in the work log and `linux-port-frontier.md`.

## Result

The fixed-width telemetry change is complete. The x64 probe reached the next
execution-layer error, while the Linux i386 and Win32 regression targets passed. The
next work unit is the x86-64 guest-entry and stack-bridge design.
