# 작업 로그 20260906-609 — Linux x64 LINEXE direct-dispatch capability fallback

## 결과

Linux x64에서 host direct-dispatch thunk가 없을 때 선택적 Glide patch의
실패가 전체 LINEXE 초기화 실패로 전파되던 문제를 수정했습니다. 기본
실행은 base trap/HLE Glide gate image를 유지하고 LINEXE 환경을 활성화합니다.

## 구현

`RunExecutionThread`와 loader가 direct-dispatch 정책 요청, host capability,
실제 활성 상태를 분리해서 계산합니다. Linux x64에서는 thunk가 없으므로
요청은 true이지만 실제 direct dispatch는 false입니다. 이 경우
`PatchGlideGatePlanForDirectDispatch`를 호출하지 않고 검증된 기본 gate plan을
기록합니다. i386 host에서 thunk가 제공되는 기존 direct patch 경로는
변경하지 않았습니다.

`REPIU_LINEXE_INIT_TRACE`는 `requested`, `capable`, `direct`를 모두
기록하고, loader는 `requested/capable/enabled`를 기록합니다.

## 검증

빌드:

```text
wsl.exe -d Ubuntu-24.04 -- bash -lc "cd /mnt/e/MYWORK/Projects/rePIU && cmake --build build/linux_x64_debug --target repiu repiu_core_probe -j 2"
```

core probe:

```text
core_probe_total=24
core_probe_failures=0
core_probe_all=true
core_probe_skipped=2 stack_bridge guest_stack_switch
core_probe_host=x64 (Task 545: i386 assembly probes are not built)
```

Linux x64 기본 실행:

```text
[loader] Win32 AOT-DBT Glide gate direct dispatch requested/capable/enabled: true/false/false
[repiu-linexe-init] extracted=1 plan=1 layout=1 glide_fits=1 requested=1 capable=0 direct=0 writes=1/1/1/1/1 verify=1 descriptors=1 protect=1/1/1/1/1 active=1
[repiu-dos-int-context] phase=return eip=0x010F17EE eax=0x0000FFFF ... gs=0x0020 linexe=1
```

이후 guest는 `INT 31h AX=0006`을 실행하고, `CON`을 handle `0x0005`로
열어 `0x2F` 바이트를 성공적으로 썼습니다. 그러나 기존
`Not enough memory to allocate file structures` 메시지를 출력하고
`AX=4C01h`로 종료했습니다. 이 종료는 direct-dispatch fallback 실패나
`CON` open 실패가 아닙니다. 다음 분석 대상은 DPMI `AX=0006` 또는 그 결과를
사용하는 guest 초기화 경로입니다.

실행 중 `[repiu-fault]`, `Segmentation fault`, `Illegal instruction`,
`core dumped`는 발생하지 않았습니다.

## English

### Result

On Linux x64, an unavailable host direct-dispatch thunk no longer propagates
the optional Glide patch failure into a failed LINEXE environment. The default
run retains the base trap/HLE Glide gate image and activates LINEXE.

### Implementation

`RunExecutionThread` and the loader now compute the direct-dispatch policy
request, host capability, and actual active state separately. Linux x64 has no
usable thunk, so the request remains true while actual direct dispatch is false.
The code skips `PatchGlideGatePlanForDirectDispatch` in that case and writes the
validated base gate plan. The existing direct patch path on i386 hosts with a
thunk is unchanged.

`REPIU_LINEXE_INIT_TRACE` reports `requested`, `capable`, and `direct`, while
the loader reports `requested/capable/enabled`.

### Verification

The Linux x64 build completed and the core probe passed `24/24`, with the two
i386 assembly probes skipped as before.

The default Linux x64 run reported:

```text
[loader] Win32 AOT-DBT Glide gate direct dispatch requested/capable/enabled: true/false/false
[repiu-linexe-init] extracted=1 plan=1 layout=1 glide_fits=1 requested=1 capable=0 direct=0 writes=1/1/1/1/1 verify=1 descriptors=1 protect=1/1/1/1/1 active=1
[repiu-dos-int-context] phase=return eip=0x010F17EE eax=0x0000FFFF ... gs=0x0020 linexe=1
```

The guest then executed `INT 31h AX=0006`, opened `CON` as handle `0x0005`,
and successfully wrote `0x2F` bytes. It still printed the existing
`Not enough memory to allocate file structures` message and exited with
`AX=4C01h`. This is not a direct-dispatch fallback failure or a `CON` open
failure. The next analysis target is DPMI `AX=0006` or the guest initialization
path consuming its result.

No `[repiu-fault]`, segmentation fault, illegal instruction, or core dump
occurred.
