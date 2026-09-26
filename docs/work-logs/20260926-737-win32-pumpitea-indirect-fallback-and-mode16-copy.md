# Task 737 작업 로그 — Win32 pumpitea 시작 크래시

설계: [20260926-737](../design/20260926-737-win32-pumpitea-indirect-fallback-and-mode16-copy.md)
작업 지시: [20260926-737](../work-orders/20260926-737-win32-pumpitea-indirect-fallback-and-mode16-copy.md)

## 요약

Win32에서 pumpitea가 약 8초에 `0xC0000005`로 죽던 원인은 하나가 아니라 세 겹이었습니다. 크래시
자체는 **i386 간접 CALL fallback이 반환 주소를 두 번 push**해서였고, 그것을 고치자 **16-bit 코드
객체 바이트를 그대로 복사한 dynamic 이미지가 전부 거절**되어 프레임이 멈췄으며, 그것을 고치자
**ISR의 200회 `in` 지연 루프가 CPU 전체**를 써서 35초 뒤 멈췄습니다. 세 가지를 고친 뒤 Win32
pumpitea는 90초 동안 크래시 없이 그리고 타이틀에 도달합니다.

| 단계 | Win32 pumpitea |
|---|---|
| 이전 | 14/14 실행이 8초에 `0xC0000005` (`0x049E26C8`, 데이터 페이지). legacy backend도 `0x040FD010`에서 crash |
| + fallback push 되돌림 | crash 없음. 그러나 40초 동안 프레임 0, DOS open 3. census: 87%가 `WaitForWorkerSignal` |
| + 16-bit 기록 INT3 방출 | 40초에 프레임 504, open 5, 번역 실패 0. 그러나 약 35초부터 정지(90초에도 504). dispatch 4.6만/초, EIP `0x04028E42` |
| + 지연 루프 inverted 형식 | **90초: 프레임 1,615, open 7(`bga\title.dat`), batch 19,001·건너뛴 읽기 376만, 20~61 fps, tick 19,002/19,519** |

## 1단계 — 범위 좁히기

* 예외 스택의 `[esp+0x18] = 0x04023E65`는 `call 0x040CA448`(glTexImage2D 래퍼) 복귀점,
  마지막 AOT 전송(#344)은 `0x04078121 → 0x0404866C`, `REPIU_AOT_TRANSFER_TARGET_TRACE=0x04049988`로
  잡은 마지막 간접 호출은 `0x0407A664 call [esi+0x9c8]`(GL 드라이버 텍스처 함수)였습니다.
  `esi = 0x049E26C8`, 즉 크래시한 "코드" 주소가 드라이버의 컨텍스트 포인터였습니다.
* 실행 probe로 `0x04049988` 진입·`0x04049ACE ret 8`·`0x0407A66A` 복귀를 찍자 복귀점의 스택
  꼭대기에 `0x0407A66A`가 **두 번** 있었습니다. 호출자의 `ret 0x18`이 두 번째 사본 아래의
  `0x049E26C8`로 돌아간 것입니다.
* `REPIU_AOT_DIRECT_RETURN_TABLE`, `..._DBT_RETURN_MISS_DISPATCH`, `..._DBT_INDIRECT`,
  `..._DBT_PORT_IO_DISPATCH`, `..._DBT_GLIDE_GATE_DISPATCH`, `..._DBT_TIMER_SAFE_POINTS`,
  `..._GUARDED_SEGMENT_LOAD/POP`, `REPIU_DISABLE_NATIVE_FAST_PATH`, `REPIU_NATIVE_LINEAR_SPAN=0`,
  `REPIU_AOT_INDIRECT_CACHE_SLOTS=1`, `..._INLINE_CACHE_PATCH_INLINE=0`: 전부 같은 주소에서 crash.
* 임시 진단으로 `ResolveAotTransferTarget`이 이 호출에서 `kTranslationFailure`를 반환함을
  확인했습니다(이후 제거).

## 2단계 — 세 결함

**간접 CALL fallback.** Task 650은 x64를 위해 push를 대상 해석 앞으로 옮겼고, i386 miss tail의
CALL fallback도 push를 남기게 했습니다. i386의 legacy fallback은 CALL을 원본 그대로 재실행하므로
push가 두 번 됩니다. `HandleAotIndirectTransfer`가 해석 실패 시 i386에서만 ESP와 call frame을
되돌리고, miss tail은 CALL도 `LEA ESP,[ESP+8]`을 씁니다. Task 650의 probe는
`indirect_fallback_call_stack_restored`로 기대값을 바꿨습니다.

**16-bit 코드 복사.** 번역 실패 메시지에 단계와 첫 decode 실패 항목을 넣자
`image: emitted code cache failed decode verification (first at guest 0x04110001: 0 of 2 bytes …
emitted=0024)`. `0x04110000`은 object 3(pumpit2a의 Task 692와 같은 16-bit 스택 전환 stub)이고,
i386 `kCopy`가 16-bit 기록을 그대로 복사했습니다. Linux의 long-mode emitter는 같은 기록을
INT3으로 거절하므로 Linux에서는 이 실패가 없었습니다. i386도 INT3 경계로 방출합니다.

**지연 루프.** pumpitea의 입력 스캔은 `inc ebx; sub eax,eax; in ax,dx; cmp ebx,200;
jge exit; jmp back`입니다. Task 414 batcher는 `jl back` 형식만 받았습니다. 앞으로 가는
`jge`/`jg` 뒤에 짧은 `jmp`가 오면 조건을 뒤집어 기존 규칙에 넘깁니다. 본문·EAX 0화·카운터 검증은
그대로이며, 배치 뒤 guest가 마지막 반복(`ebx=200`의 IN)을 직접 실행합니다.

## 검증

| 검증 | 결과 |
|---|---|
| core probe | Win32 30/30, Linux x64 32/32 (`indirect_fallback_call_stack_restored=true`) |
| Win32 pumpitea 90초 | 위 표. `REPIU_GLIDE_FRAME_RATE_LOG`가 79초까지 20~61 fps |
| Win32 pumpit2a 30초 × 2 | 정상 teardown, 프레임 986·991, tick 5,626/6,119·5,651/6,205, 번역 실패 0 |
| Linux x64 pumpitea 30초 × 2 | 폴트 0, 프레임 1,190·1,213(이전 35초에 841), batch 6,280, tick 98% |
| Linux x64 pumpit2a 30초 × 3 | 폴트 0, batcher 일치 0(루프 모양이 다름) |

Linux pumpit2a의 첫 40초 실행 한 번은 예산 만료 시점(`elapsed 40000`)에 `signal=0xb rip=0x200246`으로
끝났습니다. Task 730이 기록한 기존 teardown 결함의 서명이며 이후 3회에서 재현되지 않았습니다.

## 남은 것

1. Win32 legacy backend(`REPIU_EXECUTION_BACKEND=legacy`)의 pumpitea crash(`0x040FD010`)는 다시
   확인하지 않았습니다. 기본 backend가 아닙니다.
2. Win32 pumpitea는 이제 타이틀에 도달합니다. 곡 선택·플레이 진행은 확인하지 않았습니다.
3. `ARCHITECTURE.md`의 "간접 CALL fallback stack 의미" 절이 두 번 들어 있습니다. 두 곳 모두에
   정정을 붙였고 중복 자체는 정리하지 않았습니다.

---

# English

# Task 737 work log — the Win32 pumpitea start-up crash

Design: [20260926-737](../design/20260926-737-win32-pumpitea-indirect-fallback-and-mode16-copy.md)
Work order: [20260926-737](../work-orders/20260926-737-win32-pumpitea-indirect-fallback-and-mode16-copy.md)

## Summary

pumpitea's `0xC0000005` about 8 s into a Win32 run had three layers. The crash itself was the **i386
indirect CALL fallback pushing the return address twice**; with that fixed, **every dynamic image that
copied a 16-bit code object's bytes was rejected** and no frame was drawn; with that fixed, **the
ISR's 200-iteration `in` delay loop took the whole CPU** and the game stalled after 35 s. With all
three fixed, Win32 pumpitea runs 90 s without a crash and reaches the title.

| Stage | Win32 pumpitea |
|---|---|
| Before | 14 of 14 runs die at 8 s with `0xC0000005` (`0x049E26C8`, a data page); the legacy backend dies at `0x040FD010` |
| + undo the fallback push | no crash, but no frame in 40 s, 3 DOS opens; census: 87% in `WaitForWorkerSignal` |
| + INT3 for 16-bit records | 504 frames in 40 s, 5 opens, 0 translation failures; stalls from about 35 s (still 504 at 90 s), 46 k dispatches/s at EIP `0x04028E42` |
| + inverted delay-loop form | **90 s: 1,615 frames, 7 opens (`bga\title.dat`), 19,001 batches skipping 3.76 M reads, 20-61 fps, ticks 19,002/19,519** |

## Stage 1 — narrowing

The exception stack's `[esp+0x18] = 0x04023E65` is the return point of `call 0x040CA448` (the
glTexImage2D wrapper); the last AOT transfer (#344) was `0x04078121 → 0x0404866C`; and
`REPIU_AOT_TRANSFER_TARGET_TRACE=0x04049988` caught the last indirect call, `0x0407A664 call
[esi+0x9c8]` into the GL driver's texture function, with `esi = 0x049E26C8` — the "code" address of
the crash was the driver's context pointer. Execution probes at `0x04049988`, `0x04049ACE ret 8` and
the `0x0407A66A` return showed `0x0407A66A` **twice** at the top of the stack; the caller's `ret 0x18`
returned to the `0x049E26C8` beneath the second copy. Thirteen feature switches, turned off one at a
time, all crashed at the same address. A temporary diagnostic (removed since) confirmed
`ResolveAotTransferTarget` answering `kTranslationFailure` for this call.

## Stage 2 — the three defects

**Indirect CALL fallback.** Task 650 moved the push ahead of target resolution for x64 and had the
i386 miss tail's CALL fallback keep it. An i386 legacy fallback re-executes the CALL natively, so the
push happens twice. `HandleAotIndirectTransfer` now restores ESP and pops the call frame on i386 when
resolution fails, and the miss tail uses `LEA ESP,[ESP+8]` for CALL as well. Task 650's probe became
`indirect_fallback_call_stack_restored`.

**16-bit code copy.** With the stage and first decode-failure sample in the message —
`image: emitted code cache failed decode verification (first at guest 0x04110001: 0 of 2 bytes …
emitted=0024)` — the culprit was object 3, the same 16-bit stack-switch stub Task 692 found in
pumpit2a, copied verbatim by i386 `kCopy`. Linux's long-mode emitter already refuses the record with
an INT3, which is why Linux never saw this; i386 now emits the boundary too.

**Delay loop.** pumpitea's input scan is `inc ebx; sub eax,eax; in ax,dx; cmp ebx,200; jge exit; jmp
back`, and Task 414's batcher accepted only the `jl back` form. A forward `jge`/`jg` followed by a
short `jmp` is now inverted and handed to the existing rules; the body, EAX-zeroing and counter checks
are unchanged, and after a batch the guest still runs the final iteration (the IN at `ebx=200`).

## Verification

Core probe: Win32 30/30, Linux x64 32/32 (`indirect_fallback_call_stack_restored=true`). Win32
pumpitea 90 s: the table above, with `REPIU_GLIDE_FRAME_RATE_LOG` showing 20-61 fps to 79 s. Win32
pumpit2a 2 × 30 s: clean teardowns, 986 and 991 frames, ticks 5,626/6,119 and 5,651/6,205, zero
translation failures. Linux x64 pumpitea 2 × 30 s: no faults, 1,190 and 1,213 frames (841 in 35 s
before), 6,280 batches, 98% of ticks. Linux x64 pumpit2a 3 × 30 s: no faults, zero batcher matches
(its loop has another shape). One earlier 40-second Linux pumpit2a run ended at budget expiry
(`elapsed 40000`) with `signal=0xb rip=0x200246`, the signature of the pre-existing teardown fault
recorded in Task 730; it did not recur in three further runs.

## What remains

1. The Win32 legacy backend's pumpitea crash (`REPIU_EXECUTION_BACKEND=legacy`, `0x040FD010`) was
   not re-checked; it is not the default backend.
2. Win32 pumpitea now reaches the title; song select and play were not checked.
3. `ARCHITECTURE.md` contains its "Indirect CALL fallback stack semantics" section twice. The
   correction was appended to both; the duplication itself was left alone.
