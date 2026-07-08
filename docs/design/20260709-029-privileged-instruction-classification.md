# privileged instruction 분류 설계

## 배경

현재 `piu_1st` 실행 경로는 Win32 x86 host에서 guest ESP 전환 후 원본 entry로 진입한다.
첫 중단 지점은 relocated address `0x020F3890`의 Win32 예외 `0xC0000096`이다.

이 예외는 privileged instruction 계열이지만, 아직 다음 두 가능성 중 어느 쪽인지 코드가 구분하지 않는다.

1. DOS/4G 런타임이 의도적으로 실행하는 명령이며 HLE trap으로 처리해야 하는 경우
2. 실행 전 CPU/DPMI 상태 초기화가 부족해서 원본 코드가 기대한 환경과 달라진 경우

## 설계

HLE 모듈에 작은 privileged instruction classifier를 추가한다.
입력은 relocated image byte window와 focus offset이며, 출력은 다음 정보를 가진다.

* 명령 시작 byte
* mnemonic
* 명령 길이
* HLE trap 후보 여부
* CPU/DPMI 상태 초기화 후보 여부
* 분류 메시지

초기 classifier는 DOS/4G 부트스트랩과 DPMI 경계에서 먼저 만날 수 있는 명령만 다룬다.

* `CLI`, `STI`, `HLT`
* `INT imm8`
* `LGDT`, `LIDT`, `LLDT`, `LTR`
* `MOV` to/from control register
* `IN`, `OUT`, `INS`, `OUTS`

이 단계에서는 명령을 에뮬레이션하지 않는다.
loader가 예외 발생 시 기존 byte window 출력 뒤에 분류 결과를 출력하여 다음 구현 판단을 돕는다.

## 판단 기준

`CLI`, `STI`, `HLT`, `INT imm8`, port I/O는 DOS/DPMI/HLE trap 후보로 본다.
이 명령들은 modern user-mode host에서 직접 실행할 수 없지만, 원본 코드 흐름에서는 DOS extender나 DOS service boundary일 가능성이 높다.

descriptor table 조작이나 control register 접근은 CPU/DPMI 상태 초기화 후보로 본다.
이 경우 단순 skip보다 descriptor abstraction, DPMI API, 또는 더 이른 runtime 초기화가 필요할 수 있다.

## 검증

* Win32 x86 loader를 빌드한다.
* `repiu_loader_win32.exe piu_1st` 실행에서 기존 `0xC0000096` 예외 관찰이 유지되는지 확인한다.
* 출력에 privileged instruction classification이 포함되는지 확인한다.
* `repiu_loader_win32.exe dos4gw_hello` 실행에서 `Hello, world!` 출력이 유지되는지 확인한다.

# Privileged Instruction Classification Design

## Background

The current `piu_1st` path enters the original entry after switching to the guest ESP in the Win32 x86 host.
The first stop is Win32 exception `0xC0000096` at relocated address `0x020F3890`.

The exception is a privileged-instruction class exception, but the code does not yet distinguish between these possibilities.

1. The DOS/4G runtime intentionally executes the instruction and it should become an HLE trap.
2. The pre-entry CPU/DPMI state is incomplete compared with the environment expected by the original code.

## Design

Add a small privileged instruction classifier to the HLE module.
The input is a relocated image byte window and focus offset, and the output contains:

* instruction start byte
* mnemonic
* instruction length
* HLE trap candidate flag
* CPU/DPMI state initialization candidate flag
* classification message

The initial classifier only handles instructions likely to appear first around DOS/4G bootstrap and DPMI boundaries.

* `CLI`, `STI`, `HLT`
* `INT imm8`
* `LGDT`, `LIDT`, `LLDT`, `LTR`
* `MOV` to/from control register
* `IN`, `OUT`, `INS`, `OUTS`

This step does not emulate the instruction.
The loader prints the classification after the existing byte window when an exception occurs, giving the next implementation step a stable input.

## Judgment

`CLI`, `STI`, `HLT`, `INT imm8`, and port I/O are treated as DOS/DPMI/HLE trap candidates.
They cannot run directly in a modern user-mode host, but in the original flow they are likely DOS extender or DOS service boundaries.

Descriptor-table manipulation or control-register access is treated as a CPU/DPMI state initialization candidate.
That path may require descriptor abstraction, DPMI APIs, or earlier runtime initialization rather than simply skipping the instruction.

## Verification

* Build the Win32 x86 loader.
* Run `repiu_loader_win32.exe piu_1st` and confirm the existing `0xC0000096` observation remains.
* Confirm that privileged instruction classification is printed.
* Run `repiu_loader_win32.exe dos4gw_hello` and confirm `Hello, world!` is preserved.
