# DOS Current Directory HLE 설계

## 배경

`piu_1st`는 runtime memory arena 확장 이후 `0x020F5637`의 `CD 21`에서 중단된다. 직전 명령이 `B4 3B`이므로 이 호출은 DOS `INT 21h AH=0x3B` current directory 변경 요청이다.

이 중단 지점은 opcode 문제가 아니라 DOS 파일시스템 상태 HLE 요구사항이다.

## 정책

Host process의 current directory는 변경하지 않는다. DOS HLE runtime 안에 가상 current directory를 유지하고, `chdir`, `getcwd`, `open`, `findfirst` 같은 후속 DOS 파일 호출은 모두 이 가상 current directory를 기준으로 경로를 해석한다.

Target profile의 `working_directory`를 DOS 가상 드라이브의 루트로 사용한다. Guest 경로는 DOS 구분자와 drive prefix를 정규화한 뒤, 최종 host 경로가 이 루트 밖으로 나가지 못하게 제한한다.

## `INT 21h AH=0x3B`

`DS:DX`가 가리키는 ASCIZ 경로를 읽는다. 현재 trace에서는 `EDX`가 relocated runtime arena 안의 linear pointer로 관측되므로, 이번 단계에서는 `EDX`를 직접 guest linear address로 읽는다.

처리 결과:

* 대상 경로가 target working directory 아래의 실제 디렉터리이면 성공, CF clear.
* 경로가 없거나 디렉터리가 아니면 실패, CF set, `AX=0x0003`.
* 루트 밖 탈출이 감지되면 실패, CF set, `AX=0x0005`.
* guest 문자열을 읽을 수 없으면 실패, CF set, `AX=0x0003`.

## 범위

이번 작업은 `chdir`과 경로 해석 상태를 추가한다. 실제 파일 open/read/findfirst 구현은 후속 작업으로 남긴다.

# DOS Current Directory HLE Design

## Background

After the runtime memory arena expansion, `piu_1st` stops at `CD 21` at `0x020F5637`. The preceding instruction is `B4 3B`, so this is a DOS `INT 21h AH=0x3B` current-directory change request.

This stop is not an opcode problem. It is a DOS filesystem-state HLE requirement.

## Policy

Do not change the host process current directory. Keep a virtual current directory inside the DOS HLE runtime, and make later DOS file calls such as `chdir`, `getcwd`, `open`, and `findfirst` resolve paths through that virtual current directory.

Use the target profile `working_directory` as the root of the virtual DOS drive. Normalize DOS separators and drive prefixes, then prevent the final host path from escaping that root.

## `INT 21h AH=0x3B`

Read the ASCIZ path pointed to by `DS:DX`. In the current trace, `EDX` is observed as a linear pointer inside the relocated runtime arena, so this step reads `EDX` directly as a guest linear address.

Result handling:

* If the target path is an existing directory under the target working directory, succeed and clear CF.
* If the path does not exist or is not a directory, fail, set CF, and return `AX=0x0003`.
* If escaping above the root is detected, fail, set CF, and return `AX=0x0005`.
* If the guest string cannot be read, fail, set CF, and return `AX=0x0003`.

## Scope

This task adds `chdir` and path-resolution state. Actual file open/read/findfirst handling remains follow-up work.
