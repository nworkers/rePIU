# piu_1st stage.cfg 누락 경로 resize guard 설계

## 배경

`piu_1st`는 `\DATAS\BGA`로 current directory를 변경한 뒤 `intro.ani`와 `stage.cfg` 열기를 시도한다.

이번 작업에서는 `stage.cfg`가 실제로 없는 파일을 찾는 정상 probe라고 간주한다. 따라서 DOS open 실패 `0x0002` 자체를 성공으로 바꾸거나 host asset을 추가하지 않는다.

## 관측

현재 `stage.cfg` open 실패 이후 마지막 resize 요청은 `INT 21h AH=0x4A`, `ES=0x0024`, `BX=0x4AE1`이며 성공으로 처리된다.

그 직후 `0x020F7340`의 `C7 01 FF FF FF FF`에서 access violation이 발생한다. 이 명령은 `ECX`가 가리키는 주소에 `0xFFFFFFFF`를 쓰는 일반 memory write이고, 예외 시점의 `ECX=0x0264AE04`는 현재 arena end `0x025E7000`을 넘어선다.

이 지점은 opcode 자체를 HLE로 대신 실행할 문제가 아니다. 원본 런타임이 resize 성공 응답을 믿고 arena 밖 heap metadata를 갱신하려는 상태로 보는 편이 안전하다.

## 정책

전체 DOS MCB 관리자는 아직 만들지 않는다. 이번 단계에서는 관측된 `piu_1st` 흐름에 한해 다음 resize를 실패 처리한다.

* 마지막 DOS open이 `stage.cfg` 실패이고, `guest_es == 0x0024`이며, `BX >= 0x4AE1`이면 실패, CF set, `AX=0x0008`, `BX=0x4AE0`.
* 기존 `BX > 0xE700` guard는 유지한다.
* `stage.cfg` open 실패는 그대로 DOS error `0x0002`로 남긴다.
* traced DOS HLE에 `INT 21h AH=0x4C` process terminate를 연결해 resize 실패 복구 경로가 종료를 요청할 때 host trampoline으로 돌아오게 한다.
* loader 로그와 `scripts/test_all.ps1` 기대값은 새 다음 관측 지점에 맞춰 갱신한다.

이 정책은 `stage.cfg` 누락을 자산 오류로 보지 않고, 누락 후 원본 런타임이 밟는 resize 실패/복구 흐름을 관찰하기 위한 임시 guard이다.

구현 후 새 관측점은 `spr.res` open 실패 이후의 `0x020F7340` write이다. `SPR.RES`는 target root에는 존재하지만 current directory `\DATAS\BGA` 아래에는 없으므로, 다음 작업은 root asset fallback 또는 current-directory 정책을 별도로 설계해야 한다.

# piu_1st stage.cfg Missing-Path Resize Guard Design

## Background

`piu_1st` changes the current directory to `\DATAS\BGA`, then attempts to open `intro.ani` and `stage.cfg`.

This task treats `stage.cfg` as a legitimate probe for a file that is actually absent. Therefore, the DOS open failure `0x0002` is not converted into success, and no host asset is added.

## Observation

After the current `stage.cfg` open failure, the last resize request is `INT 21h AH=0x4A`, `ES=0x0024`, `BX=0x4AE1`, and it is currently treated as success.

Immediately after that, execution hits an access violation at `C7 01 FF FF FF FF` at `0x020F7340`. This instruction is a normal memory write of `0xFFFFFFFF` to the address in `ECX`; at the exception point, `ECX=0x0264AE04`, which is beyond the current arena end `0x025E7000`.

This should not be handled by emulating the opcode itself. It is safer to treat it as the original runtime trusting an overly successful resize response and attempting to update heap metadata outside the arena.

## Policy

Do not build a complete DOS MCB manager yet. For this step, fail only the observed `piu_1st` resize flow:

* If the last DOS open was the failed `stage.cfg` probe, `guest_es == 0x0024`, and `BX >= 0x4AE1`, fail with CF set, `AX=0x0008`, and `BX=0x4AE0`.
* Keep the existing `BX > 0xE700` guard.
* Preserve the `stage.cfg` open failure as DOS error `0x0002`.
* Connect `INT 21h AH=0x4C` process termination in traced DOS HLE so the resize-failure recovery path can return to the host trampoline when it requests process termination.
* Update loader logs and `scripts/test_all.ps1` expectations for the new next observation point.

This is a temporary guard for observing the original runtime's resize-failure recovery path after the missing `stage.cfg` probe.

After implementation, the new observation point is the `0x020F7340` write after a failed `spr.res` open. `SPR.RES` exists at the target root but not under current directory `\DATAS\BGA`, so the next task should separately design root asset fallback or current-directory policy.
