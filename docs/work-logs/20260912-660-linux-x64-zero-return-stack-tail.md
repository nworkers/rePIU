# Task 660 작업 로그: Linux x64 zero-return stack tail

## 한국어

### 작업 개요

Task 659에서 `ENTER 4,0`의 host-stack 오염을 제거한 뒤 새로 확인된
zero-return frontier를 조사하기 위해, 기존 bounded guest stack ring을
zero-return frame 진단에서도 출력하도록 연결했습니다. 원본 guest 코드,
instruction lowering, resolver의 target 선택, fail-closed 정책은 변경하지
않았습니다.

### 변경 내용

- `TraceLinuxX64ZeroReturnFrame()`에 기존
  `TraceLinuxX64ReturnStackTail()` 호출을 추가했습니다.
- 함수의 선행 선언을 추가하여 zero-return 진단 경로에서 동일한 bounded
  helper를 사용할 수 있게 했습니다.
- `REPIU_LINUX_X64_RETURN_STACK_TAIL`이 없으면 추가 출력이 없으며, 값이
  설정된 경우에도 기존 ring capacity 제한과 요청 개수 제한을 그대로
  사용합니다.

### 검증

Linux x64 Debug 빌드와 core probe를 실행했습니다.

```text
./scripts/build_linux_x64.sh --config Debug --build-dir build/linux_x64_debug --target repiu --target repiu_core_probe
./build/linux_x64_debug/repiu_core_probe
```

빌드는 성공했으며, 기존의 `g_repiu_active_thread_context` `extern`
경고만 다시 출력되었습니다. probe 결과는 다음과 같습니다.

```text
core_probe_total=27
core_probe_failures=0
core_probe_all=true
```

다음 bounded 실행으로 zero-return frame과 stack tail의 연결을 확인했습니다.

```text
timeout 20s env REPIU_STALL_TIMEOUT_MS=0 REPIU_EXECUTION_TIMEOUT_MS=60000 REPIU_GLIDE_SWAP_INTERVAL=0 REPIU_LINUX_X64_STACK_TRACE=1 REPIU_LINUX_X64_RETURN_FRAME_TRACE=1 REPIU_LINUX_X64_RETURN_STACK_TAIL=96 ./build/linux_x64_debug/repiu pumpit2a 2>&1
```

확인된 핵심 출력은 다음과 같습니다.

```text
[repiu-x64-return-frame] ... source=0x00000000 ... guest_esp=0x0158C864 ... status=0x010F0237 ... m8=0x010EFE5F m4=0x00000000 p0=0x00000001 p4=0x00000001 matches=0x2 ... call_depth=1024 ...
[repiu-x64-return-stack-tail] n=1 target=0x00000000 sequence=14497674 printed=96
```

tail에는 다음과 같은 시간 순서의 stack operation이 포함되었습니다.

```text
site=0x010EFE55 guest-push ... value=0x010EFE2C
site=0x010EFE5A direct-call fallthrough=0x010EFE5F ... value=0x010EFE5F
site=0x010EFEC0 guest-push ... value=0x00000000
site=0x010EFEC1 guest-push ... value=0x010FB81E
site=0x010EFEC2 guest-push ... value=0x00000001
site=0x010EFEC3 guest-push ... value=0x0158C92C
site=0x010EFEC7 direct-call fallthrough=0x010EFECC ... value=0x010EFECC
```

실행은 기존과 같이 `RepiuLinuxX64ReturnThunk`의 unresolved target에
대한 fail-closed `SIGTRAP`에서 종료되었습니다. 이는 이번 변경으로
새로 발생한 오류가 아니라, zero target을 임의로 복구하지 않은 결과입니다.

### 결과와 남은 frontier

이번 작업으로 zero-return frame 직후에 최근 stack write ring을 bounded
형태로 볼 수 있게 되었습니다. 따라서 반복 재사용된 반환 슬롯의 직접
writer뿐 아니라 그 이전의 push/call 순서도 확인할 수 있습니다. 다만
zero target의 원인은 아직 확정하지 않았습니다.

현재 다음 조사 지점은 `guest EIP=0x010F0237`의 `RET`와 그 직전 함수
epilogue인 `0x010F022D`–`0x010F0237` 구간입니다. frame 출력의
`call_depth=1024`는 추적 상태가 포화되었음을 보여 주지만, 원인으로
단정하지 않았습니다.

## English

### Work summary

After Task 659 removed the host-stack corruption from `ENTER 4,0`, a new
zero-return frontier remained. This task connected the existing bounded guest
stack ring to the zero-return frame diagnostic. It did not change original
guest code, instruction lowering, resolver target selection, or the
fail-closed policy.

### Changes

- Added a call to the existing `TraceLinuxX64ReturnStackTail()` from
  `TraceLinuxX64ZeroReturnFrame()`.
- Added the forward declaration needed by the diagnostic path.
- Kept the default output unchanged when
  `REPIU_LINUX_X64_RETURN_STACK_TAIL` is unset; configured output still uses
  the existing ring-capacity and requested-count limits.

### Verification

The Linux x64 Debug build and core probe were run with the commands shown in
the Korean section. The build succeeded, with only the existing
`g_repiu_active_thread_context` declared-and-initialized-as-`extern` warning.
The probe reported:

```text
core_probe_total=27
core_probe_failures=0
core_probe_all=true
```

A bounded `pumpit2a` run with `REPIU_LINUX_X64_STACK_TRACE=1`,
`REPIU_LINUX_X64_RETURN_FRAME_TRACE=1`, and
`REPIU_LINUX_X64_RETURN_STACK_TAIL=96` confirmed that the tail follows the
zero-return frame:

```text
[repiu-x64-return-frame] ... source=0x00000000 ... guest_esp=0x0158C864 ... status=0x010F0237 ... m8=0x010EFE5F m4=0x00000000 p0=0x00000001 p4=0x00000001 matches=0x2 ... call_depth=1024 ...
[repiu-x64-return-stack-tail] n=1 target=0x00000000 sequence=14497674 printed=96
```

The chronological tail included the `0x010EFE55` guest push,
`0x010EFE5A` direct-call fallthrough `0x010EFE5F`, the four pushes at
`0x010EFEC0`–`0x010EFEC3`, and the `0x010EFEC7` direct-call fallthrough
`0x010EFECC`.

Execution still ended at the unresolved-target fail-closed `SIGTRAP` in
`RepiuLinuxX64ReturnThunk`. This is the preserved diagnostic failure path,
not a new failure introduced by Task 660.

### Result and remaining frontier

The zero-return frame now exposes a bounded recent stack-write window. This
shows the push/call sequence before the reused return slot in addition to the
slot's direct writers. The root cause of the zero target remains unresolved.

The next investigation point is the `RET` at guest `EIP=0x010F0237` and the
preceding function epilogue at `0x010F022D`–`0x010F0237`. The
`call_depth=1024` value indicates saturated tracing state, but it has not been
identified as causal.
