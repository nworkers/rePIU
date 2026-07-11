# Allocator shadow metadata 89 store 설계

## 배경

allocator 실패 sentinel `C7 01 FF FF FF FF`를 처리한 뒤 `piu_1st`는 relocated base + `0x000F7AA8`의 `89 17`에서 중단된다. 명령은 `mov dword ptr [edi], edx`이고 예외 시점 값은 다음과 같다.

* `EDI=0x026E49C4`
* `EDX=0x00000490`
* 기존 shadow sentinel address: `0x026E4E54`

sentinel address와 destination의 차이는 정확히 `0x490`이며 `EDX`와 같다. 이어지는 명령은 `89 06`, `89 5F 04`, `89 4F 08`, `89 7B 08`로 allocator block header의 인접 필드를 초기화하는 흐름으로 보인다.

## 설계

기존 `89 /r` memory-store decoder를 재사용하되 성공한 파일 경로의 out-of-arena store는 다음 allocator metadata 관계에서만 shadow memory에 기록한다.

* 기존 shadow memory range가 있어야 한다.
* destination과 store 끝은 runtime arena end 이후 1 MiB 범위 안에 있어야 한다.
* 첫 header store는 기존 shadow minimum보다 앞에 있으며 `shadow_min - destination == value`여야 한다.
* 후속 store는 확장된 shadow range 내부이거나 현재 maximum 바로 다음 dword에서 시작해야 한다.

이 조건은 현재 관측된 allocator block과 인접 필드만 연결한다. 임의의 `89 /r` out-of-arena store, SIB addressing, register destination은 계속 거부한다.

## 검증

* Win32 x86 빌드
* `piu_1st` 수동 실행으로 연속 metadata store 및 다음 blocker 관측
* 전체 현재 테스트

# Allocator Shadow Metadata 89 Store Design

## Background

After handling the allocator failure sentinel `C7 01 FF FF FF FF`, `piu_1st` stops at `89 17` at relocated base + `0x000F7AA8`. The instruction is `mov dword ptr [edi], edx`, with:

* `EDI=0x026E49C4`
* `EDX=0x00000490`
* existing shadow sentinel address: `0x026E4E54`

The difference between the sentinel and destination is exactly `0x490`, matching `EDX`. The following instructions are `89 06`, `89 5F 04`, `89 4F 08`, and `89 7B 08`, which appear to initialize adjacent allocator block-header fields.

## Design

Reuse the existing `89 /r` memory-store decoder, but record an out-of-arena store after a successful file path only when it has the following allocator metadata relationship:

* An existing shadow-memory range must exist.
* The destination and store end must remain within 1 MiB after the runtime arena end.
* The first header store must precede the existing shadow minimum and satisfy `shadow_min - destination == value`.
* A following store must start inside the expanded shadow range or at the next dword immediately after its current maximum.

This connects only the observed allocator block and adjacent fields. Arbitrary out-of-arena `89 /r` stores, SIB addressing, and register destinations remain rejected.

## Verification

* Build Win32 x86.
* Run `piu_1st` manually to observe consecutive metadata stores and the next blocker.
* Run the current full test set.
