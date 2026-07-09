# Traced DOS Write HLE 설계

## 배경

DOS IOCTL 처리 후 `piu_1st`는 `0x020F6476`의 `CD 21`에서 다시 중단된다. 실행 로그의 레지스터는 `EAX=0x00004002`, `EBX=0x00000001`, `ECX=0x0000000A`, `EDX=0x025D6EC8`이며, 이는 DOS `INT 21h AH=0x40` write 요청이다.

일반 DOS HLE 경로에는 이미 `AH=0x40` 처리가 있지만, `piu_1st`가 사용하는 traced DOS HLE 경로에는 아직 같은 분기가 없다.

## 정책

traced DOS HLE에서 `AH=0x40`은 기존 일반 DOS HLE와 동일하게 처리한다.

* `BX`는 대상 handle로 유지한다.
* `CX`는 write byte count로 사용한다.
* `EDX`는 현재 관측된 linear guest pointer로 보고 runtime arena 안의 버퍼를 읽는다.
* 버퍼를 읽을 수 있으면 HLE console output에 누적한다.
* DOS 관례에 맞춰 성공 시 CF를 clear하고 `AX=CX`를 반환한다.

이번 단계는 콘솔 출력 캡처와 실행 전진을 위한 최소 구현이다. 파일 handle에 대한 실제 host write는 후속 파일 I/O HLE에서 다룬다.

# Traced DOS Write HLE Design

## Background

After DOS IOCTL handling, `piu_1st` stops again at `CD 21` at `0x020F6476`. The execution log shows `EAX=0x00004002`, `EBX=0x00000001`, `ECX=0x0000000A`, and `EDX=0x025D6EC8`, which is a DOS `INT 21h AH=0x40` write request.

The general DOS HLE path already handles `AH=0x40`, but the traced DOS HLE path used by `piu_1st` does not yet have the same branch.

## Policy

In traced DOS HLE, `AH=0x40` follows the existing general DOS HLE behavior.

* Keep `BX` as the target handle.
* Use `CX` as the write byte count.
* Treat `EDX` as the currently observed linear guest pointer into the runtime arena.
* If the buffer is readable, append it to HLE console output.
* On success, clear CF and return `AX=CX` per DOS convention.

This is a minimum implementation for console output capture and execution progress. Real host writes for file handles belong to later file I/O HLE work.
