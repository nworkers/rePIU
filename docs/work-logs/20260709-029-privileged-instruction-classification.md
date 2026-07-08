# privileged instruction 분류 작업 로그

## 결과

HLE 모듈에 privileged instruction classifier를 추가했다.
classifier는 relocated image byte window와 focus offset을 입력으로 받아 opcode, mnemonic, instruction length, HLE trap 후보 여부, CPU/DPMI 상태 초기화 후보 여부를 보고한다.

초기 지원 범위는 다음과 같다.

* `CLI`, `STI`, `HLT`
* `INT imm8`
* port I/O 명령
* `LGDT`, `LIDT`, `LLDT`, `LTR`, `LMSW`, `INVLPG`
* control register `MOV`

Win32 loader는 예외 발생 시 기존 relocated byte window 뒤에 classifier 결과를 출력한다.
`docs/TODO.md`는 privileged instruction 분류 완료와 다음 실제 trap 처리 작업을 반영하도록 갱신했다.

## 검증

* `cmd /c scripts\build_win32_x86.bat`: 성공
* `build\vs2022_win32_debug\Debug\repiu_loader_win32.exe piu_1st`: 성공
  * exception code: `0xC0000096`
  * exception address: `0x020F3890`
  * focused opcode: `0xFB`
  * mnemonic: `STI`
  * instruction class: `HLE trap candidate`
  * CPU/DPMI state candidate: `false`
* `build\vs2022_win32_debug\Debug\repiu_loader_win32.exe dos4gw_hello`: 성공
  * `Win32 HLE console output:` 아래 `Hello, world!` 출력 유지

## 판단

현재 첫 privileged instruction은 descriptor table 조작이나 control register 접근이 아니라 `STI`이다.
따라서 이번 관찰 지점은 CPU/DPMI 상태 초기화 부족보다는 DOS/4G 런타임의 interrupt flag 제어를 HLE trap으로 처리해야 하는 경로로 보는 것이 타당하다.

다음 작업은 이 분류 결과를 실제 HLE dispatcher handler 호출/복귀 규약에 연결하고, `STI`를 처리한 뒤 guest 실행을 계속 진행시키는 것이다.

# Privileged Instruction Classification Work Log

## Result

Added a privileged instruction classifier to the HLE module.
The classifier receives a relocated image byte window and focus offset, then reports opcode, mnemonic, instruction length, HLE trap candidacy, and CPU/DPMI state initialization candidacy.

The initial supported set is:

* `CLI`, `STI`, `HLT`
* `INT imm8`
* port I/O instructions
* `LGDT`, `LIDT`, `LLDT`, `LTR`, `LMSW`, `INVLPG`
* control-register `MOV`

The Win32 loader now prints classifier results after the existing relocated byte window when an exception occurs.
`docs/TODO.md` was updated to record completion of privileged instruction classification and the next real trap handling work.

## Verification

* `cmd /c scripts\build_win32_x86.bat`: passed
* `build\vs2022_win32_debug\Debug\repiu_loader_win32.exe piu_1st`: passed
  * exception code: `0xC0000096`
  * exception address: `0x020F3890`
  * focused opcode: `0xFB`
  * mnemonic: `STI`
  * instruction class: `HLE trap candidate`
  * CPU/DPMI state candidate: `false`
* `build\vs2022_win32_debug\Debug\repiu_loader_win32.exe dos4gw_hello`: passed
  * Preserved `Hello, world!` under `Win32 HLE console output:`

## Judgment

The first privileged instruction is `STI`, not a descriptor-table operation or control-register access.
This observation is therefore better treated as a DOS/4G runtime interrupt-flag operation that should become an HLE trap, rather than as missing CPU/DPMI state initialization.

The next task is to connect this classification result to the real HLE dispatcher handler call/return convention and continue guest execution after handling `STI`.
