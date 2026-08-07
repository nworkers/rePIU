# TODO

## 2026-08-07 무진행 감시견이 건강한 Glide 실행을 죽입니다 (Task 445에서 발견, 별도 태스크 예정)

`PollThreadUntilExit`에는 1초짜리 무진행 감시견이 있고
(`src/platform/win32/telemetry/live_telemetry_snapshot.cpp:474-485`),
`timeout_milliseconds != INFINITE`일 때만 무장합니다. Task 435가 기본 timeout을
`0`(무제한)으로 바꾼 뒤로 평소 실행에서는 아예 무장되지 않아 드러나지 않았습니다.

**증상.** `REPIU_EXECUTION_TIMEOUT_MS=40000`으로 pumpit2를 돌리면 40초가 아니라
**6.27초**에 `timed_out=true`로 끝납니다. 게임은 그 순간까지 정상이었습니다 — buffer
swap 811회, 삼각형 48,882개, batch 평균 6.09.

**근인.** 감시견이 보는 "진행"은 `diagnostic_progress_count`,
`single_step_trace_count`, `aot_boundary_count + aot_reentry_count` 세 가지뿐인데,
**Glide 게이트 직접 디스패치 경로(`AOT-DBT Glide gate direct dispatch`)가 저 셋 중
어느 것도 올리지 않습니다.** 끊기기 직전 구간(6000ms 기준 6266ms)에서 heartbeat는
+768, `dispatch_entry`는 +384였는데 세 카운터는 전부 그대로였습니다. 게스트가 살아서
게이트를 때리는 중인데 감시견 눈에는 정지로 보인 것입니다.

**영향 범위.** 상한을 거는 자동화(CI, 회귀 스크립트, A/B 캡처)는 전부 이 지뢰를 밟습니다.
기본값 무제한으로 사람이 창을 닫아 끝내는 수동 실행만 안전합니다.

**고칠 방향(둘 중 하나).**

* `progressed` 판정에 Glide 게이트/디스패치 카운터를 포함시킵니다. 감시견의 "진행"
  정의를 실제 실행 경로 집합과 일치시키는 쪽입니다.
* 벽시계 상한과 무진행 감시견을 서로 다른 설정으로 분리합니다. 지금은 전자를 켜면
  후자가 딸려 오는데, 둘은 다른 질문에 답하는 장치입니다.

## 2026-08-07 The stall watchdog kills healthy Glide runs (found during Task 445, deferred)

`PollThreadUntilExit` carries a one-second no-progress watchdog at
`src/platform/win32/telemetry/live_telemetry_snapshot.cpp:474-485`, armed only when
`timeout_milliseconds != INFINITE`. Since Task 435 made the default timeout `0`, meaning
unlimited, it never arms in ordinary runs, which is why it went unnoticed.

Running pumpit2 with `REPIU_EXECUTION_TIMEOUT_MS=40000` ends at **6.27 seconds**, not
forty, with `timed_out=true`, while the game was rendering normally to that point: 811
buffer swaps, 48,882 triangles, a mean batch of 6.09. The watchdog's notion of progress
is only `diagnostic_progress_count`, `single_step_trace_count` and
`aot_boundary_count + aot_reentry_count`, and **the Glide gate direct dispatch path
raises none of them**. Over the final interval heartbeat rose by 768 and
`dispatch_entry` by 384 while all three stayed frozen: the guest was alive and calling
gates, and the watchdog read that as a stall.

Anything that sets a bound -- CI, regression scripts, A/B captures -- hits this. Only
manual runs left unlimited and closed by hand are safe. The fix is either to include the
Glide gate and dispatch counters in the `progressed` test, so the watchdog's definition
of progress matches the set of paths execution actually takes, or to separate the
wall-clock limit from the stall watchdog into independent settings, since turning on the
former currently drags in the latter and the two answer different questions.

## 2026-07-30 Glide setter 생략 기본값 결정 보류 (Task 365)

Task 365가 batch 1의 7종 setter(`grColorMask`, `grAlphaBlendFunction`,
`grClipWindow`, `grAlphaTestFunction`, `grFogMode`, `grCullMode`,
`grDepthBufferFunction`)에서 동일 상태의 host rendezvous 생략을 **기본 ON**으로
넣었습니다. `REPIU_GLIDE_SETTER_ELIDE=0`으로 복원합니다.

정확성은 증명됐고(관측된 중복만 생략, 렌더 시퀀스 phase offset +1에서 72.9% 완전 일치)
비용도 확실히 줄었으나(rendezvous 41,368회 제거, Glide gate -5.13%p), **부팅 포함
자동 60초 장면에서는 프레임이 변하지 않았습니다**(1,215 → 1,206, 편차 내).

따라서 기본값 유지 여부는 **사용자가 실제 FPS 급락 장면에서 측정한 뒤** 결정합니다.
절차는 [검증 가이드](guides/glide-setter-elision-testing.md)에 있습니다.

같은 이유로 다음 항목도 함께 보류합니다.

* **batch 2 생략 확장** — `grDepthMask`(반복률 72.63%),
  `grConstantColorValue`(77.67%), `grTexClampMode`/`FilterMode`/`MipMapMode`(99.73%).
  현재 장면이 setter 경로에 제한되지 않으므로 같은 결과가 예상됩니다. 사용자 측정에서
  프레임 개선이 확인되면 재개합니다.
* **triangle 제출 batching** — 원래 Task 366 계획이었으나 "비용을 줄여도 프레임이 늘지
  않는다"는 신호가 두 번(Task 335, 365) 나왔으므로 pacing 귀속 뒤로 미룹니다.
* **`grTexSource` 생략** — 반복률 32.24%, 최대 연속 3회로 이득이 거의 없어 제외
  상태를 유지합니다.

## 2026-07-30 Deferred: deciding the Glide setter elision default (Task 365)

Task 365 enabled exact-state host-rendezvous elision by default for seven batch-one
setters, with `REPIU_GLIDE_SETTER_ELIDE=0` restoring the original path. Correctness
is proven and the cost reduction is real (41,368 rendezvous removed, Glide gate down
5.13 points), but frames did not move in the boot-inclusive automated scene.

The default therefore stays undecided until measured in a real scene where FPS
actually collapses; the procedure is in
[the testing guide](guides/glide-setter-elision-testing.md). Batch two (`grDepthMask`,
`grConstantColorValue`, and the texture clamp/filter/mipmap setters) and triangle
submission batching are deferred behind that same measurement, because two
independent results now show cost removal not converting into throughput.
`grTexSource` stays excluded on its own merits at 32.24% repetition.


## 2026-07-11 Port I/O 0x02A0 계열 의미 분석 보류

`piu_1st`에서 `0x02A0` 계열 Port I/O trace를 수집한 결과, 다음 패턴이 관측되었다.

* `0x02AC <- 0x00000010`
* `0x02A0 <- 0x00000001`
* `0x02A2 <- 0x00000000`
* `0x02A0 <- 0x00000005`
* `0x02A2 <- 0x00000000`
* 이후 `0x02A0` 값이 `+4`씩 증가하고 `0x02A2 <- 0`이 반복된다.

이 패턴은 index/data 형태의 register 초기화 또는 작은 I/O register block처럼 보이지만, 실제 장치 의미는 아직 확정하지 않는다. 현재까지 `IN` 응답이 관측되지 않았으므로, 보안 장치나 응답형 하드웨어로 단정하지 않는다.

다음 단계에서는 이 항목을 당장 구현하지 않고 TODO로 보류한다. 이후 필요할 때 `0x02A0` 계열 trace를 더 길게 수집하거나, `OUT DX,EAX` wrapper의 caller를 추적해서 어떤 코드가 index/value를 구성하는지 분석한다.

## 2026-07-11 Port I/O 0x02A0-Family Meaning Deferred

The `piu_1st` Port I/O trace for the `0x02A0` family showed this pattern:

* `0x02AC <- 0x00000010`
* `0x02A0 <- 0x00000001`
* `0x02A2 <- 0x00000000`
* `0x02A0 <- 0x00000005`
* `0x02A2 <- 0x00000000`
* Then the `0x02A0` value increases by `+4`, with `0x02A2 <- 0` repeated between writes.

This looks like indexed register initialization or a small I/O register block, but the actual device meaning is not confirmed yet. No `IN` response has been observed so far, so do not classify it as a security device or response-driven hardware yet.

For now, defer this item as TODO instead of implementing it. If needed later, collect a longer `0x02A0` family trace or trace the caller of the `OUT DX,EAX` wrapper to identify which code builds the index/value sequence.

## 2026-07-10 traced opcode HLE timeout 관측 진행

`stage.cfg`는 `\DATAS\BGA` 아래에 없는 파일을 찾는 정상 probe로 간주한다. 이후 `spr.res`도 current directory 기준 `\DATAS\BGA\SPR.RES`가 없어서 DOS error `0x0002`로 실패한다. root fallback은 추가하지 않았다.

이번 작업에서는 관측된 opcode를 계속 처리했다. `89 /r`, `C7 /0`, `66 C7 /0`, `D9` FPU memory load/store, `8B /r` shadow memory load를 추가했고, out-of-arena store는 byte-addressed shadow memory에 기록하도록 했다.

현재는 더 이상 새 opcode 예외가 잡히지 않고 `piu_1st`가 minimal execution timeout에 도달한다. 다음 작업은 opcode 추가가 아니라 timeout된 guest 실행의 마지막 `EIP` 또는 주기적 실행 trace를 안전하게 기록하는 관측 장치를 설계하는 것이다.

## 2026-07-10 Traced Opcode HLE Timeout Observation Progress

`stage.cfg` is treated as a legitimate probe for a file absent under `\DATAS\BGA`. After that, `spr.res` also fails with DOS error `0x0002` because `\DATAS\BGA\SPR.RES` is absent relative to the current directory. Root fallback was not added.

This task continued handling observed opcodes. It added `89 /r`, `C7 /0`, `66 C7 /0`, `D9` FPU memory load/store, and `8B /r` shadow memory load handling. Out-of-arena stores are now recorded in byte-addressed shadow memory.

There is no longer a newly caught opcode exception. `piu_1st` now reaches the minimal execution timeout. The next task is not adding another opcode, but designing an observation tool that safely records the last guest `EIP` or a periodic execution trace for the timed-out guest execution.

## 2026-07-09 DOS file open HLE 진행

`INT 21h AH=0x3D` file open 요청을 가상 DOS current directory 기반 path resolver로 처리했다. `piu_1st`는 `\DATAS\BGA` current directory 상태에서 `intro.ani`를 열려고 하며, 이는 `\DATAS\BGA\INTRO.ANI`로 해석된다. 해당 host file은 존재하지 않으므로 DOS error `0x0002`로 실패 처리된다.

현재 다음 중단 지점은 `0x020FAA81`의 `CD 21`이다. 직전 명령이 `B4 44`이므로 이는 `INT 21h AH=0x44` DOS IOCTL 요청으로 분류된다. 다음 작업은 trace에서 관측된 IOCTL subfunction과 handle 의미를 확인하고 최소 HLE 응답을 설계하는 것이다.

## 2026-07-09 DOS File Open HLE Progress

`INT 21h AH=0x3D` file open requests are now handled through the virtual DOS current-directory path resolver. In `piu_1st`, `intro.ani` is opened while the virtual current directory is `\DATAS\BGA`, so it resolves to `\DATAS\BGA\INTRO.ANI`. That host file does not exist, so the request fails with DOS error `0x0002`.

The current next stop is `CD 21` at `0x020FAA81`. The preceding instruction is `B4 44`, so this is classified as a DOS IOCTL request through `INT 21h AH=0x44`. The next task is to inspect the traced IOCTL subfunction and handle meaning, then design the minimal HLE response.

## 2026-07-09 DOS current-directory HLE 진행

`INT 21h AH=0x3B` current-directory 변경 요청을 가상 DOS current directory로 처리했다. `piu_1st`는 `\datas\bga` 요청을 target working directory 아래의 `\DATAS\BGA`로 성공 처리한 뒤 다음 지점까지 진행한다.

현재 다음 중단 지점은 `0x020F740C`의 `CD 21`이다. 직전 명령이 `B4 3D`이므로 이는 `INT 21h AH=0x3D` DOS open 요청으로 분류된다. 다음 작업은 current-directory 기반 path resolver를 재사용해 DOS file open handle HLE를 설계하는 것이다.

## 2026-07-09 DOS Current-Directory HLE Progress

`INT 21h AH=0x3B` current-directory changes are now handled through the virtual DOS current directory. `piu_1st` successfully resolves `\datas\bga` to `\DATAS\BGA` under the target working directory and advances to the next point.

The current next stop is `CD 21` at `0x020F740C`. The preceding instruction is `B4 3D`, so this is classified as a DOS open request through `INT 21h AH=0x3D`. The next task is to design DOS file-open handle HLE on top of the current-directory path resolver.

## 2026-07-09 런타임 메모리 아레나 진행

`INT 21h AH=0x4A` 이후 관측된 `0x020F8405`의 `C7 04 02 FF FF FF FF` write blocker는 runtime memory arena reserve를 `0x005E7000`으로 확장하면서 통과했다. 현재 arena는 relocated base `0x02000000`, image reserve `0x005D7000`, expansion slack `0x00010000`, arena end `0x025E7000`을 사용한다.

현재 다음 중단 지점은 `0x020F5637`의 `CD 21`이다. 직전 명령이 `B4 3B`이므로 이는 `INT 21h AH=0x3B` DOS current directory 변경 요청으로 분류된다. 다음 작업은 opcode 구현이 아니라 DOS 파일시스템/current-directory HLE 정책을 정리하는 것이다.

## 2026-07-09 Runtime Memory Arena Progress

The previous `C7 04 02 FF FF FF FF` write blocker at `0x020F8405`, observed after `INT 21h AH=0x4A`, is now passed by expanding the runtime memory arena reserve to `0x005E7000`. The current arena uses relocated base `0x02000000`, image reserve `0x005D7000`, expansion slack `0x00010000`, and arena end `0x025E7000`.

The current next stop is `CD 21` at `0x020F5637`. The preceding instruction is `B4 3B`, so this is classified as a DOS current-directory change request through `INT 21h AH=0x3B`. The next task is not opcode implementation; it is a DOS filesystem/current-directory HLE policy.

## 2026-07-09 DS low-memory 읽기 HLE 진행

`8B 06` / `mov eax, dword ptr ds:[esi]` 중단 지점을 segment HLE에서 처리했다.
이어지는 command-line 또는 DOS low-memory probe 흐름에서 `80 3E 00`, `AC`, `A4`도 DS shadow selector와 `ESI < 0x10000` 조건으로 0을 반환하도록 처리했다.

`INT 21h AH=0xED`도 최소 응답으로 처리했다.
`INT 21h AH=0x4A`도 일반 DOS HLE와 같은 최소 성공 응답으로 처리했다.
현재 다음 중단 지점은 `0x020F8405`의 `C7 04 02 FF FF FF FF`이다. 이 명령은 `EDX + EAX` 위치에 `0xFFFFFFFF`를 쓰려는 일반 메모리 write이며, 관측된 대상 주소 `0x025D7E54`는 현재 relocated placement 끝 `0x025D7000`을 넘어선다.
따라서 다음 작업은 opcode 특례가 아니라 `INT 21h AH=0x4A` 이후 DOS 메모리 블록 resize/heap 확장 정책을 설계하는 것이다.

## 2026-07-09 DS Low-Memory Read HLE Progress

The `8B 06` / `mov eax, dword ptr ds:[esi]` stop is now handled by segment HLE.
The following command-line or DOS low-memory probe flow also handles `80 3E 00`, `AC`, and `A4` as zero reads/copies when the DS shadow selector exists and `ESI < 0x10000`.

`INT 21h AH=0xED` is also handled with a minimal response.
`INT 21h AH=0x4A` is also handled with the same minimal success response as the general DOS HLE path.
The current next stop is `C7 04 02 FF FF FF FF` at `0x020F8405`. This instruction attempts a normal memory write of `0xFFFFFFFF` to `EDX + EAX`; the observed target address `0x025D7E54` is beyond the current relocated placement end `0x025D7000`.
Therefore, the next task is not an opcode special case. It needs a design for DOS memory block resize/heap expansion policy after `INT 21h AH=0x4A`.

## 2026-07-09 segment register store HLE 진행

`66 26 8C 1D` segment register store 중단 지점은 guest selector shadow state를 relocated runtime memory에 쓰는 HLE 요구사항으로 분류되었다.
이번 작업에서는 이 지점을 직접 처리하여 다음 중단 지점을 관찰한다.

## 2026-07-09 Segment Register Store HLE Progress

The `66 26 8C 1D` segment-register store stop is classified as an HLE requirement that writes guest selector shadow state into relocated runtime memory.
This task handles that stop directly and observes the next execution stop.

## 2026-07-09 segment register store HLE 완료

`66 26 8C 1D` segment register store는 처리되었고, `DS=0x0024`가 relocated destination `0x020F3AED`에 기록되는 것을 확인했다.
다음 중단 지점은 `0x020F39C8`의 `66 8E 05 E4 65 1A 02` memory-source segment register load이다.

## 2026-07-09 Segment Register Store HLE Complete

`66 26 8C 1D` segment-register store is handled, and `DS=0x0024` is written to relocated destination `0x020F3AED`.
The next stop is the memory-source segment-register load `66 8E 05 E4 65 1A 02` at `0x020F39C8`.

## 2026-07-09 memory-source segment register load HLE 완료

`66 8E 05 E4 65 1A 02` memory-source segment register load는 처리되었고, relocated source `0x021A65E4`에서 읽은 selector `0x0024`가 guest `ES`에 기록되는 것을 확인했다.
다음 중단 지점은 `0x020F39DD`의 `26 8A 4F FF` segment override byte memory load이다.

## 2026-07-09 Memory-Source Segment Register Load HLE Complete

`66 8E 05 E4 65 1A 02` memory-source segment-register load is handled, and selector `0x0024` read from relocated source `0x021A65E4` is recorded into guest `ES`.
The next stop is the segment-override byte memory load `26 8A 4F FF` at `0x020F39DD`.

## 2026-07-09 현재 상태

이전 TODO/PLAN의 분석 및 기반 구조 작업은 완료되었다.
이번 단계에서 Win32 x86 guest ESP 전환 trampoline도 구현되었다.
privileged instruction 예외 위치를 HLE trap 후보와 CPU/DPMI 상태 초기화 후보로 분류하는 초기 classifier도 추가되었다.
`STI`는 첫 HLE trap으로 처리되었다.
`INT 21h AH=0x30`도 처리되어 다음 중단 지점인 `INT 21h AH=0xFF`까지 진행된다.
`INT 21h AH=0xFF`도 최소 응답으로 처리되어 다음 중단 지점인 segment register load 계열 명령 `8E D9`까지 진행된다.
segment register load는 guest selector shadow state로 처리되어 다음 중단 지점인 segment register store 계열 명령 `66 26 8C 1D`까지 진행된다.

남은 실제 구현 작업은 다음과 같다.

1. HLE dispatcher handler 호출 규약과 guest context 복귀 경로 구현
2. 실제 trace로 확인된 INT21/INT31 서비스 최소 구현
3. selector/descriptor 권한 검사와 DPMI descriptor API 연결
4. `66 26 8C 1D` segment register store 중단 지점을 selector/descriptor memory write HLE 요구사항으로 분류

## Status As Of 2026-07-09

The previous TODO/PLAN analysis and foundation work is complete.
This step also implements the Win32 x86 guest ESP-switching trampoline.
It also adds the initial classifier that separates privileged-instruction exceptions into HLE trap candidates and CPU/DPMI state initialization candidates.
`STI` is now handled as the first HLE trap.
`INT 21h AH=0x30` is also handled, allowing execution to proceed to the next stop at `INT 21h AH=0xFF`.
`INT 21h AH=0xFF` is also handled with a minimal response, allowing execution to proceed to the next stop at segment-register load instruction `8E D9`.
Segment-register loads are handled through guest selector shadow state, allowing execution to proceed to the next stop at segment-register store instruction `66 26 8C 1D`.

Remaining real implementation work:

1. Implement the HLE dispatcher handler calling convention and guest context return path.
2. Implement only the INT21/INT31 services confirmed by actual traces.
3. Connect selector/descriptor permission checks and DPMI descriptor APIs.
4. Clarify and implement the segment-override byte memory load policy for `26 8A 4F FF`.

## 현재 우선순위 상태

2026-07-08 기준으로 이전 TODO/PLAN 잔여 작업은 `docs/20260708-todo-plan-results.md`에 정리했다.

1. Skipped relocation 10개 상세 분석: 완료. dry-run 상세 목록과 analyzer 출력 추가.
2. guest stack 전환 trampoline 설계: 완료. 결과 문서에 장기 trampoline 순서 정리.
3. INT/DPMI/HLE trap 진입 방식 설계: 완료. 결과 문서에 SEH/HLE dispatcher 방향 정리.
4. 원본 entry 최소 실행 예외 분석: 완료. `0x020F3890` / `0xC0000096`을 privileged instruction trap 후보로 분류.
5. Win32 object protection 정책 정밀화: 완료. LE flags 기반 정책 유지 및 비 Win32 unsupported stub 추가.
6. relocated image runtime memory manager 승격: 완료. 장기 manager 입력 데이터와 다음 확장 범위 정리.

## 구현 보완 완료 상태

1. 예외 EIP 주변 relocated image byte window 기능: 완료.
2. guest context 구조체와 guest stack switch plan 구조: 완료.
3. HLE dispatcher table 초안: 완료.
4. selector/descriptor table 최소 모델: 완료.
5. privileged instruction 초기 분류기와 loader 출력 연결: 완료.
6. `STI` HLE trap 처리와 다음 `INT 21h` 중단 지점 관찰: 완료.
7. `INT 21h AH=0x30` DOS version query 처리와 다음 `AH=0xFF` 중단 지점 관찰: 완료.
8. `INT 21h AH=0xFF` 최소 처리와 다음 `8E D9` 중단 지점 관찰: 완료.
9. `8E /r` register source segment load 처리와 다음 `66 26 8C 1D` 중단 지점 관찰: 완료.

## 남은 실제 구현 작업

1. HLE dispatcher handler 호출 규약과 guest context 복귀 경로 구현.
2. INT21/INT31 중 실제 trace로 확인된 서비스부터 최소 구현.
3. selector/descriptor 권한 검사와 DPMI descriptor API 연결.
4. `66 26 8C 1D` segment register store를 guest selector shadow state와 relocated memory write 정책으로 연결.

## Current Priority Status

As of 2026-07-08, the previous TODO/PLAN remaining work is summarized in `docs/20260708-todo-plan-results.md`.

1. Detailed analysis of the 10 skipped relocations: complete. Added dry-run detail list and analyzer output.
2. Guest stack switching trampoline design: complete. Recorded the long-running trampoline sequence in the result document.
3. INT/DPMI/HLE trap entry design: complete. Recorded the SEH/HLE dispatcher direction in the result document.
4. Minimal original entry exception analysis: complete. Classified `0x020F3890` / `0xC0000096` as a privileged-instruction trap candidate.
5. Win32 object protection policy refinement: complete. Kept LE flags policy and added non-Win32 unsupported stubs.
6. Relocated image runtime memory manager promotion: complete. Recorded long-running manager input data and next extension scope.

## Implementation Follow-up Completion Status

1. Relocated image byte window around exception EIP: complete.
2. Guest context and guest stack switch plan structures: complete.
3. HLE dispatcher table draft: complete.
4. Minimal selector/descriptor table model: complete.
5. Initial privileged instruction classifier and loader output wiring: complete.
6. `STI` HLE trap handling and observation of the next `INT 21h` stop: complete.
7. `INT 21h AH=0x30` DOS version query handling and observation of the next `AH=0xFF` stop: complete.
8. `INT 21h AH=0xFF` minimal handling and observation of the next `8E D9` stop: complete.
9. `8E /r` register-source segment load handling and observation of the next `66 26 8C 1D` stop: complete.
10. `66 26 8C /r` absolute-destination segment store handling and observation of the next `66 8E 05` stop: complete.
11. `66 8E /r` absolute-source segment load handling and observation of the next `26 8A 4F FF` stop: complete.

## Remaining Real Implementation Work

1. Implement the HLE dispatcher handler calling convention and guest context return path.
2. Implement only the INT21/INT31 services confirmed by actual traces.
3. Connect selector/descriptor permission checks and DPMI descriptor APIs.
4. Clarify and implement the `[8B] 06` DS-based low-memory or descriptor-based 32-bit memory read through selector shadow state and address translation policy.

## 2026-07-09 segment override byte memory load HLE 완료

`26 8A 4F FF` segment override byte memory load를 HLE로 처리했다.
현재 관찰된 형태는 `ES:[EDI - 1]`이며, `ES=0x0024`, `EDI=0x00000081` 상태에서 DOS command tail length byte인 `ES:0x80`을 읽는 것으로 분류했다.
이 값은 빈 command tail로 보고 `0x00`을 반환한다.

다음 중단 지점은 `0x020F4DAC`의 `[8B] 06`이다.
직전에는 `DS=0x002C`가 load되며, 예외 시점의 `ESI=0x00000000` 상태에서 DS 기반 low-memory 또는 descriptor 기반 32-bit memory read 정책이 필요하다.

## 2026-07-09 Segment Override Byte Memory Load HLE Complete

Handled the `26 8A 4F FF` segment-override byte memory load through HLE.
The observed form is `ES:[EDI - 1]`; with `ES=0x0024` and `EDI=0x00000081`, this is classified as a DOS command tail length byte read at `ES:0x80`.
The value is treated as an empty command tail and returns `0x00`.

The next stop is `[8B] 06` at `0x020F4DAC`.
Immediately before the stop, `DS=0x002C` is loaded, and the exception state has `ESI=0x00000000`, so the next policy needed is a DS-based low-memory or descriptor-based 32-bit memory read.
