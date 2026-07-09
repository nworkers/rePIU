# DOS Resize Guard 설계

## 배경

`piu_1st`는 DOS IOCTL과 traced write를 통과한 뒤 `0x020F7340`의 일반 memory write에서 중단된다. 단순히 runtime arena slack을 늘리면 같은 write 대상 주소가 새 arena 끝 뒤로 함께 이동하므로, 문제는 slack 부족이 아니라 `INT 21h AH=0x4A` resize 요청을 무조건 성공 처리하는 임시 정책에 있다.

## 관측

원래 64KB arena slack 상태에서 마지막 resize 요청은 `ES=0x0024`, `BX=0xE7E1`이다. 이 요청을 성공 처리하면 곧바로 `0x025E7E04`에 metadata write를 시도하며, 이는 현재 arena end `0x025E7000`을 넘어선다.

## 정책

이번 단계에서는 전체 DOS MCB 관리자를 만들지 않고, 관측된 `piu_1st` resize 흐름에만 상한을 둔다. DOS resize의 핵심 입력은 `BX` paragraph 수와 resize 대상 block selector/segment이다. 현재 실행 경로는 Win32 `CONTEXT`의 segment register보다 HLE가 추적하는 guest segment shadow를 더 신뢰할 수 있으므로, `guest_es` shadow와 `BX`를 함께 사용한다.

처리 정책은 다음과 같다.

* `guest_es == 0x0024`이고 `BX > 0xE700`이면 실패, CF set, `AX=0x0008`, `BX=0xE700`.
* 그 외 현재 관측 범위의 요청은 기존처럼 성공, CF clear.
* 모든 요청은 마지막 selector, paragraph, 결과, error code로 기록한다.

`0xE700`은 현재 `0xE7E1` 요청이 arena 밖 metadata write로 이어지는 것을 막기 위한 관측 기반 임시 상한이다. 이후에는 selector base와 arena committed range를 연결한 일반 resize 정책으로 교체한다.

# DOS Resize Guard Design

## Background

After passing DOS IOCTL and traced write handling, `piu_1st` stops at a normal memory write at `0x020F7340`. Simply increasing runtime arena slack makes the write target move along with the new arena end, so the issue is not insufficient slack. The root cause is the temporary policy that always succeeds `INT 21h AH=0x4A` resize requests.

## Observation

With the original 64KB arena slack, the last resize request is `ES=0x0024`, `BX=0xE7E1`. Succeeding this request immediately leads to a metadata write to `0x025E7E04`, which is beyond the current arena end `0x025E7000`.

## Policy

This step does not build a full DOS MCB manager. Instead, it adds a guard for the observed `piu_1st` resize flow. The key DOS resize inputs are the requested paragraph count in `BX` and the block selector/segment being resized. In the current execution path, the HLE-tracked guest segment shadow is more reliable than Win32 `CONTEXT` segment registers, so use the `guest_es` shadow together with `BX`.

Handling policy:

* If `guest_es == 0x0024` and `BX > 0xE700`, fail with CF set, `AX=0x0008`, and `BX=0xE700`.
* Other currently observed requests continue to succeed with CF clear.
* Every request is recorded with the last selector, paragraph count, result, and error code.

`0xE700` is an observed temporary limit that prevents the current `0xE7E1` request from leading to an out-of-arena metadata write. Later work should replace this with a general resize policy connected to selector base and the committed arena range.
