# TODO

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
