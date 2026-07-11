# Arena 외부 allocator sentinel store 설계

## 배경

`INT 21h AH=3Eh` 파일 닫기 처리를 통과한 `piu_1st`는 relocated base + `0x000F86E0`의 `C7 01 FF FF FF FF`에서 접근 위반으로 중단된다. 이 명령은 `mov dword ptr [ecx], 0xFFFFFFFF`이며 예외 시점의 `ECX`는 `0x026E4E54`이다.

현재 relocated runtime arena는 base `0x02000000`, reserve size `0x006D7000`, end `0x026D7000`이다. 따라서 대상 주소는 arena 끝보다 `0xDE54` 뒤에 있다. 기존 `C7 /0` HLE가 명령을 인식하지 못한 것이 아니라, 성공한 `intro.ani` 파일 경로 뒤에 발생한 out-of-arena 쓰기를 실패 경로용 shadow memory 정책이 의도적으로 거부한 결과이다.

2 MiB slack을 시험 적용한 단독 실행은 더 진행했지만, 전체 테스트에서는 guest가 추가 공간을 사용한 뒤 `ECX=0x0283CE54`에서 같은 명령으로 다시 중단되었다. 고정 slack 확대는 중단점을 이동할 뿐 근본 해결이 아니다.

## 설계

기존 `C7 /0` memory-store HLE에 allocator 실패 sentinel 예외를 추가한다.

* 명령이 `C7 /0` dword immediate store이고 값이 `0xFFFFFFFF`인 경우만 대상으로 한다.
* destination이 arena end 이상이며 end 이후 1 MiB 이내인 경우만 allocator 실패 sentinel로 인정한다.
* 해당 store를 기존 shadow memory에 기록하고 원본 명령 길이만큼 진행한다.
* 실제 arena 내부 store는 계속 네이티브로 실행되며, 그 밖의 out-of-arena store는 계속 거부한다.

기존의 “마지막 DOS open 실패” 조건은 파일 실패 경로의 일반 shadow store를 위해 유지한다. 새 조건은 성공한 파일 경로에서도 나타나는 allocator 실패 sentinel만 별도로 허용한다.

## 검증

* Win32 x86 빌드
* `piu_1st` 수동 실행에서 arena 크기와 다음 중단점 관측
* 전체 현재 테스트 실행 및 기대값 갱신

# Out-of-Arena Allocator Sentinel Store Design

## Background

After passing `INT 21h AH=3Eh` file close handling, `piu_1st` stops with an access violation at `C7 01 FF FF FF FF` at relocated base + `0x000F86E0`. The instruction is `mov dword ptr [ecx], 0xFFFFFFFF`, and exception-time `ECX` is `0x026E4E54`.

The current relocated runtime arena has base `0x02000000`, reserve size `0x006D7000`, and end `0x026D7000`. The destination is therefore `0xDE54` bytes beyond the arena. The existing `C7 /0` HLE recognizes the instruction, but intentionally rejects this out-of-arena write because it follows a successful `intro.ani` file path rather than the failure-path shadow-memory policy.

An experimental 2 MiB slack allowed a standalone run to advance, but the full test run consumed the added space and stopped on the same instruction with `ECX=0x0283CE54`. Increasing a fixed slack only moves the stop and is not a root fix.

## Design

Add an allocator-failure-sentinel exception to the existing `C7 /0` memory-store HLE.

* Accept only a `C7 /0` dword immediate store whose value is `0xFFFFFFFF`.
* Require the destination to be at or above the arena end and within 1 MiB after it.
* Record that store in existing shadow memory and advance by the original instruction length.
* Continue to execute stores inside the real arena natively and reject other out-of-arena stores.

Keep the existing last-DOS-open-failure condition for general failure-path shadow stores. The new condition separately permits only the allocator failure sentinel observed after successful file paths.

## Verification

* Build Win32 x86.
* Run `piu_1st` manually and observe the arena size and next stop.
* Run the current full test set and update expectations.
