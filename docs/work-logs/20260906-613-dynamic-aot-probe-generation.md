# Task 613 작업 로그 — Linux x64 동적 AOT probe generation

## 한국어

### 구현

* 최신 dynamic append 범위의 active map entry만 대상으로 하는
  `InstallAotProbeSentinelInLatestAppend`를 추가했습니다.
* `RequestAotDynamicTranslation`이 append 성공을 받은 직후
  `REPIU_EXECUTION_PROBE_OFFSET` target을 새 generation에 INT3로 patch하도록
  연결했습니다. patch는 두 generation에서 `installed=1`로 확인되었습니다.
* Linux x64의 `CopySnapshotFromContextRecord`가 fixed-width guest context
  fields를 복사하도록 수정했습니다. Windows native x64 `CONTEXT` 경계는
  유지하고, Linux x64의 `Eax/Eip/EFlags/Esp`는 이미 signal adapter가
  materialize한 값을 그대로 읽습니다.

### 검증

Linux x64 debug `repiu`와 `repiu_core_probe`를 다시 빌드했고, core probe는
`core_probe_total=24`, `core_probe_failures=0`, `core_probe_all=true`를
보고했습니다.

`REPIU_EXECUTION_PROBE_OFFSET=0xF1D79` 실행에서 다음을 확인했습니다.

```text
[repiu-aot-probe] dynamic guest=0x010F1D79 generation=9 ... installed=1
[repiu-aot-probe] dynamic guest=0x010F1D79 generation=10 ... installed=1
Win32 execution probe configured/hit/offset: true/true/0x000F1D79
Win32 execution probe EIP/ESP/EFLAGS: 0x010F1D79/0x0158CC3C/0x00200246
Win32 execution probe EAX/EBX/ECX/EDX: 0x00000001/0x011A7B16/0x00000000/0x00000000
```

따라서 dynamic generation patch와 Linux x64 snapshot 복사는 모두 동작합니다.
안전한 allocator branch probe도 `0x1DAC`, `0x1DB9`, `0x1DC3`, `0x1DD1`,
`0x1E0D`, `0x1E13`에서 hit했습니다. 반면 `0x1E17` direct-call,
`0x1E1C` post-call TEST, `0x4FE8` helper entry는 hit하지 않았습니다.

`0x1E13`에서 EAX=`0x0000000C`, EFLAGS=`0x00200286`이었고 해당 `JNZ`는
ZF=0으로 taken 되었습니다. guest stack의 현재 첫 dword는 `0x00000088`입니다.
map trace에서 그 앞의 원본 `mov byte ptr [esp],ah`가
`41 88 24 27`로 낮춰졌음을 확인했습니다. REX prefix가 붙은 상태에서는
ModRM reg=4가 AH가 아니라 SPL을 뜻하므로, host RSP의 low byte가 guest
local에 기록될 수 있습니다.

기본 실행은 여전히 `AX=4C01`로 fault 없이 종료되지만 기존 오류 메시지를
출력합니다. 이는 Task 613의 probe 계측이 원본 allocator 문제를 고친 것이
아니라, 다음 Task 614가 수정할 실제 x64 emission 오류를 드러냈다는
뜻입니다.

## English

### Implementation

* Added `InstallAotProbeSentinelInLatestAppend`, which considers only active map
  entries in the latest dynamic append range.
* Connected `RequestAotDynamicTranslation` so a successful append patches the
  `REPIU_EXECUTION_PROBE_OFFSET` target in the new generation immediately before
  guest resume. Both generations reported `installed=1`.
* Updated Linux x64 `CopySnapshotFromContextRecord` to copy the fixed-width guest
  context fields. The native Windows x64 `CONTEXT` boundary remains unchanged;
  Linux x64 reads the already materialized `Eax/Eip/EFlags/Esp` fields from its
  signal adapter.

### Verification

The Linux x64 Debug `repiu` and `repiu_core_probe` targets rebuilt successfully.
The core probe reported `core_probe_total=24`, `core_probe_failures=0`, and
`core_probe_all=true`.

With `REPIU_EXECUTION_PROBE_OFFSET=0xF1D79`, the run reported dynamic probe
installation in generations 9 and 10, followed by:

```text
Win32 execution probe configured/hit/offset: true/true/0x000F1D79
Win32 execution probe EIP/ESP/EFLAGS: 0x010F1D79/0x0158CC3C/0x00200246
Win32 execution probe EAX/EBX/ECX/EDX: 0x00000001/0x011A7B16/0x00000000/0x00000000
```

The dynamic patch and Linux x64 snapshot copy therefore work. Safe allocator
branch probes also hit at `0x1DAC`, `0x1DB9`, `0x1DC3`, `0x1DD1`, `0x1E0D`, and
`0x1E13`. The `0x1E17` direct call, the `0x1E1C` post-call TEST, and the
`0x4FE8` helper entry did not hit.

At `0x1E13`, EAX was `0x0000000C` and EFLAGS was `0x00200286`; the JNZ was
taken with ZF clear. The first guest-stack dword was `0x00000088`. The map
trace shows that the preceding original `mov byte ptr [esp],ah` was lowered to
`41 88 24 27`. With a REX prefix, ModRM reg=4 names SPL rather than AH, so the
host RSP low byte can be written into the guest local.

The default run still exits fault-free through `AX=4C01` with the existing error
message. Task 613 did not fix the allocator; it exposed the actual x64 emission
defect to be corrected in Task 614.
