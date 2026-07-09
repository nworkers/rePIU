# DS 메모리 읽기 HLE 설계

## 배경

`piu_1st` 실행 경로는 ES override byte load HLE 이후 `8B 06` 명령에서 다시 중단된다. 관측된 상태는 `DS=0x002C`, `ESI=0x00000000`이며, 명령은 32-bit 주소 크기 기준 `mov eax, dword ptr ds:[esi]`로 해석된다.

기존 `HandleDosMemoryAccess`에는 `8B 06`을 0으로 처리하는 임시 fallback이 있지만, 이 경로는 DOS 콘솔 샘플용 HLE가 활성화된 경우에만 사용된다. `piu_1st`는 guest stack trap 실행 경로를 사용하므로 `enable_dos_hle`가 꺼져 있고, 해당 fallback에 도달하지 않는다.

## 설계 방향

이번 단계에서는 일반적인 descriptor table 또는 DOS low-memory 모델을 새로 만들지 않는다. 대신 이미 shadow로 추적하는 guest segment selector 값을 사용하여, 현재 관측된 DS 기반 low-memory 읽기만 segment HLE의 일부로 처리한다.

첫 처리 대상은 다음 조건을 모두 만족하는 경우로 제한한다.

* opcode가 `8B 06`이다.
* 기본 segment는 DS이고, shadow `guest_ds`가 0이 아니다.
* effective offset은 `ESI`이고 현재는 `0x00000000`이다.

조건을 만족하면 `EAX=0`을 반환하고 `EIP`를 2바이트 진행한다. 이 값은 현재 실행파일이 low-memory 또는 descriptor 기반 환경 확인 값으로 읽는 미구현 DOS/DPMI 주변 상태를 보수적으로 0으로 노출하기 위한 것이다.

검증 중 `8B 06` 다음 위치에서 `80 3E 00`이 관측되었다. 이는 `cmp byte ptr ds:[esi], 0` 형태이며, 기존 DOS memory fallback도 같은 low-memory 범위를 0으로 응답한다. 따라서 DS shadow selector가 존재하고 `ESI < 0x10000`인 byte read 계열도 현재 단계의 segment HLE에 포함한다.

* `80 3E 00`: `ds:[esi]` 값을 0으로 읽고 ZF를 set, CF를 clear한다.
* `AC`: `lodsb`를 0으로 응답하고 direction flag에 따라 `ESI`를 갱신한다.
* `A4`: `movsb`를 DS low-memory 0 읽기와 현재 `EDI` 위치의 0 쓰기로 처리하고 direction flag에 따라 `ESI`/`EDI`를 갱신한다.

## 기록과 로그

기존 segment memory load 기록은 byte load만 전제로 하여 값이 `Hex8`로 출력된다. DS dword read를 명확히 구분하기 위해 기록 구조에 width를 추가하고, 로그에서도 width와 값의 폭을 함께 출력한다.

## 향후 확장

추가 명령이 발견되면 다음 순서로 확장한다.

1. 관측된 opcode, segment, offset, 반환값 의미를 작업 로그에 기록한다.
2. 범용 descriptor 해석이 필요한지 판단한다.
3. 단일 opcode 특례가 반복되면 selector 기반 memory provider 추상화를 별도 설계로 분리한다.

# DS Memory Read HLE Design

## Background

The `piu_1st` execution path stops again at opcode `8B 06` after the ES override byte load HLE. The observed state is `DS=0x002C`, `ESI=0x00000000`, and the instruction is interpreted as `mov eax, dword ptr ds:[esi]` in 32-bit address-size mode.

`HandleDosMemoryAccess` already has a temporary fallback that returns zero for `8B 06`, but that path is only active for DOS console sample HLE. `piu_1st` uses the guest stack trap execution path, so `enable_dos_hle` is disabled and the fallback is not reached.

## Design Direction

This step does not introduce a general descriptor table or DOS low-memory model. Instead, it uses the already tracked guest segment selector shadow state and handles only the currently observed DS-based low-memory reads as part of segment HLE.

The first handler is limited to all of the following conditions.

* The opcode is `8B 06`.
* The default segment is DS and shadow `guest_ds` is nonzero.
* The effective offset is `ESI`, currently `0x00000000`.

When the conditions match, the handler returns `EAX=0` and advances `EIP` by two bytes. This conservatively exposes an unimplemented DOS/DPMI surrounding-state read as zero while preserving the original executable path.

During verification, `80 3E 00` appeared immediately after `8B 06`. This is `cmp byte ptr ds:[esi], 0`, and the existing DOS memory fallback also answers zero for the same low-memory range. Therefore, DS byte-read forms with a nonzero DS shadow selector and `ESI < 0x10000` are included in this segment HLE step.

* `80 3E 00`: read `ds:[esi]` as zero, set ZF, and clear CF.
* `AC`: answer `lodsb` with zero and update `ESI` according to the direction flag.
* `A4`: handle `movsb` as a DS low-memory zero read plus a zero write to the current `EDI`, then update `ESI`/`EDI` according to the direction flag.

## Records and Logs

The existing segment memory load record assumes byte loads and logs the value as `Hex8`. To distinguish the DS dword read clearly, the record structure adds a width field, and logging reports both width and a width-appropriate value.

## Future Extension

When more instructions appear, extend in the following order.

1. Record the observed opcode, segment, offset, and return-value meaning in the work log.
2. Decide whether general descriptor interpretation is needed.
3. If one-off opcode handling repeats, split a selector-based memory provider abstraction into a separate design.
