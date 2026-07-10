# Memory store 진단 확장 설계

## 배경

`piu_1st`는 실제 environment scan 이후 `\datas\bga`로 이동하고 `spr.res` open 실패를 관측한다. 이후 실행은 memory store HLE를 통해 계속 진행되지만, 현재 로그는 마지막 store의 EIP, destination, value, applied 여부만 보여준다.

이 정보만으로는 마지막 store가 immediate store인지, register store인지, FPU memory store인지, 몇 byte 폭의 쓰기인지 분류하기 어렵다.

## 설계

Win32 trampoline의 memory store 기록에 다음 요약 필드를 추가한다.

* 마지막 store opcode
* 마지막 store byte width
* 마지막 store source kind

source kind는 진단 문자열로 저장한다. 현재 필요한 값은 다음과 같다.

* `mov-imm32`
* `mov-imm16`
* `mov-reg32`
* `fpu-m32`

기존 `applied` 필드는 유지한다. `applied=true`는 실제 guest writable range에 기록했다는 뜻이고, `applied=false`는 `spr.res` 실패 이후 shadow memory에 기록했다는 뜻이다.

## 범위 밖

* shadow memory 전체 dump는 만들지 않는다.
* guest 로직은 수정하지 않는다.
* memory allocator나 파일 HLE 정책은 변경하지 않는다.

# Memory Store Diagnostics Design

## Background

After the real environment scan, `piu_1st` changes to `\datas\bga` and observes the expected `spr.res` open failure. Execution then continues through memory store HLE, but current logs only show the last store EIP, destination, value, and whether the write was applied.

That is not enough to classify whether the last store came from an immediate store, register store, or FPU memory store, nor how wide the write was.

## Design

Add the following summary fields to Win32 trampoline memory store records.

* last store opcode
* last store byte width
* last store source kind

The source kind is stored as a diagnostic string. The currently needed values are:

* `mov-imm32`
* `mov-imm16`
* `mov-reg32`
* `fpu-m32`

Keep the existing `applied` field. `applied=true` means the value was written to a real guest writable range, while `applied=false` means it was written to shadow memory after the `spr.res` failure.

## Out Of Scope

* Do not add a full shadow memory dump.
* Do not modify guest logic.
* Do not change memory allocator or file HLE policy.
