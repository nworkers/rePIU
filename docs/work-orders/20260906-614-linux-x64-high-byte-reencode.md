# 20260906-614 Linux x64 high-byte source 재인코딩 작업 지시

## 한국어

### 목적

Linux x64 AOT에서 `REX + ModRM reg=4..7` 조합이 `AH/CH/DH/BH`가 아니라
`SPL/BPL/SIL/DIL` 계열로 해석되는 문제를 해결합니다. 현재 재현 지점인
`mov byte ptr [esp],ah`가 guest stack에 올바른 값을 쓰도록 합니다.

### 작업

1. `LongModeLowering`에 high-byte source 전용 lowering을 추가합니다.
2. Zydis operand action과 encoding을 이용해 ModRM `reg`의 source-only
   `AH/CH/DH/BH`만 분류합니다.
3. source GPR의 low/high byte 교환, `R14B` 복사, 복원, 기존 명령 재인코딩을 구현합니다.
4. high-byte destination/read-write 형태는 fail-closed refusal로 유지합니다.
5. long-mode lowering probe에 byte-level 및 execute-level 검사를 추가합니다.
6. `pumpit2a`를 Linux x64 Debug에서 재빌드하고 allocator branch/helper
   probe와 기본 실행을 확인합니다.
7. 확인된 사실과 남은 frontier를 `docs/analysis/linux-port-frontier.md`와
   작업 로그에 기록합니다.

### 완료 기준

* `mov [esp],ah`의 verdict가 새 lowering이고 emitted bytes가
  `86 C4 41 88 C6 86 C4 45 88 34 27`입니다.
* synthetic execute probe가 `AH` 값을 guest stack에 저장하고 원래 flags를
  유지합니다.
* `repiu_core_probe`가 실패 없이 통과합니다.
* Linux x64 `pumpit2a`가 이전의 high-byte 오동작을 재현하지 않고, 다음
  branch/helper frontier가 로그로 판별됩니다.
* 설계, 작업지시, 작업로그가 모두 한국어/영어로 남습니다.

### 제외 범위

* high-byte destination 및 `XCHG` 같은 read/write 교환형의 일반 lowering
* allocator/DOS memory contract를 추정하여 수정하는 작업
* 원본 guest code를 직접 패치하는 작업

## English

### Objective

Fix the Linux x64 AOT case where `REX + ModRM reg=4..7` names
`SPL/BPL/SIL/DIL` rather than `AH/CH/DH/BH`. The immediate reproduced case is
`mov byte ptr [esp],ah`, which must write the guest value to guest stack memory.

### Work items

1. Add a dedicated high-byte-source lowering to `LongModeLowering`.
2. Use Zydis operand actions and encoding to admit only source-only
   `AH/CH/DH/BH` in ModRM `reg`.
3. Emit a flag-preserving low/high-byte exchange, `R14B` copy, restore, and a
   re-encoded original operation.
4. Keep high-byte destinations and read/write forms fail-closed.
5. Add byte-level and execution-level checks to the long-mode lowering probe.
6. Rebuild Linux x64 Debug and inspect allocator branch/helper probes and the
   default run.
7. Record confirmed facts and the remaining frontier in the analysis file and
   work log.

### Done criteria

* `mov [esp],ah` receives the new lowering verdict and emits
  `86 C4 41 88 C6 86 C4 45 88 34 27`.
* The synthetic execution probe stores `AH` to guest stack memory and keeps the
  original flags.
* `repiu_core_probe` passes without failures.
* Linux x64 `pumpit2a` no longer reproduces the high-byte corruption, and the
  next branch/helper frontier is recorded.
* Design, work order, and work log are present in Korean followed by English.

### Out of scope

* General lowering for high-byte destinations or read/write exchanges such as
  `XCHG`.
* Guessing or changing the allocator/DOS memory contract.
* Patching the original guest code.
