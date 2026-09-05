# 작업 로그 20260905-594 — Linux x64 INT 31h fault 레지스터 근거

설계: [20260905-594](../design/20260905-594-linux-x64-int31-fault-register-evidence.md)

작업 지시: [20260905-594](../work-orders/20260905-594-linux-x64-int31-fault-register-evidence.md)

## 결과

Linux fault reporter가 signal handler의 로컬 `GuestCpuContext` snapshot을 받아
기존 `signal`, host `rip`, guest `eip`, fault `access` 뒤에 `eax`, `ebx`,
`ecx`, `edx`, `esi`, `edi`, `esp`, `eflags`를 32-bit hexadecimal 값으로
출력하도록 변경했습니다. 출력은 기존과 같이 `write(2)`만 사용하며,
fault resume/disposition, DPMI HLE, AOT 경로는 변경하지 않았습니다.

제공된 fault 로그는 계측 변경 전 실행 결과이므로 레지스터가 포함되어 있지
않습니다. 따라서 이번 작업에서는 `AX=0007`이 실제 fault 시점에도 유지됐다는
동적 사실을 새로 확정하지 않았습니다. 정적 분석에서 확인된 원본 경로는
`0x010F010C`의 `INT 31h`이며, 직전 명령열은 `EAX=7`을 설정합니다.

## 검증

* `git diff --check`: 통과
* 기존 Windows x86 `repiu_core_probe.exe`: `core_probe_failures=0`, 종료 코드 `0`
* Linux x64 빌드: 수행 불가. WSL 배포판 접근이 `E_ACCESSDENIED`로 실패했고,
  기존 `build/linux_x64_debug/CMakeCache.txt`는 `/mnt/e/MYWORK/Projects/rePIU`
  경로를 기록하고 있어 Windows 호스트에서 그대로 재사용할 수 없습니다.
* 보조 Windows x86 census 빌드: 기존 생성 트리의
  `GuestCpuContext` 중복 정의(`guest_address_watch.h`와
  `platform/guest_cpu_context.h`)로 실패했습니다. 이번 Linux reporter 변경에서
  발생한 오류는 아닙니다.
* watched Linux x64 재실행: 최신 Linux 바이너리를 빌드할 수 없어 보류했습니다.

## 다음 확인

WSL 또는 실제 Linux x64 환경에서 최신 소스를 빌드한 뒤 다음 실행에서
`[repiu-fault]`의 register suffix를 수집해야 합니다.

```text
REPIU_GUEST_WATCH=0x010F010C REPIU_DOS_INT_TRACE=1 ./repiu ...
```

새 로그의 `eax`, `ebx`, `ecx`, `edx`, `esp`, `eflags`를 기준으로 `INT 31h`
HLE 진입 여부와 AOT 재진입 중 레지스터 보존 여부를 판정합니다.

## English

Design: [20260905-594](../design/20260905-594-linux-x64-int31-fault-register-evidence.md)

Work order: [20260905-594](../work-orders/20260905-594-linux-x64-int31-fault-register-evidence.md)

## Result

The Linux fault reporter now receives the signal handler's local
`GuestCpuContext` snapshot and prints `eax`, `ebx`, `ecx`, `edx`, `esi`, `edi`,
`esp`, and `eflags` as 32-bit hexadecimal values after the existing `signal`,
host `rip`, guest `eip`, and fault `access` fields. It still uses only
`write(2)`, and fault disposition/resume behavior, DPMI HLE, and AOT paths are
unchanged.

The supplied fault log predates this instrumentation, so it contains no
register values. This task therefore does not newly confirm dynamically that
`AX=0007` was preserved at the fault. Static analysis still identifies the
original `INT 31h` at `0x010F010C`, with the preceding instruction sequence
setting `EAX=7`.

## Verification

* `git diff --check`: passed.
* Existing Windows x86 `repiu_core_probe.exe`: `core_probe_failures=0`, exit code `0`.
* Linux x64 build: unavailable. WSL distribution access failed with
  `E_ACCESSDENIED`, and the existing `build/linux_x64_debug/CMakeCache.txt`
  records `/mnt/e/MYWORK/Projects/rePIU`, so it cannot be reused directly from
  the Windows host.
* Auxiliary Windows x86 census build: failed on an existing generated-tree
  `GuestCpuContext` redefinition between `guest_address_watch.h` and
  `platform/guest_cpu_context.h`; this is unrelated to the Linux reporter change.
* Watched Linux x64 rerun: deferred because a current Linux binary could not be built.

## Next check

Build the current source in WSL or on a Linux x64 host and collect the register
suffix from `[repiu-fault]`, for example:

```text
REPIU_GUEST_WATCH=0x010F010C REPIU_DOS_INT_TRACE=1 ./repiu ...
```

Use the new `eax`, `ebx`, `ecx`, `edx`, `esp`, and `eflags` values to determine
whether the `INT 31h` HLE boundary was reached and whether AOT re-entry preserved
the request registers.
