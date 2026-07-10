# INT 33h AX=0000h mouse reset HLE 설계

## 배경

`INT 31h AX=0400h` trace 처리 이후 `piu_1st`는 `INT 33h` 지점에 도달했다. 처음 관측된 레지스터는 `EAX=0x00000000`이며, 이는 DOS mouse driver reset/status 호출인 `INT 33h AX=0000h`에 해당한다. 이를 처리하자 바로 `AX=0002h` mouse cursor hide 호출이 이어졌다.

현재 프로젝트 목표는 원본 32-bit x86 게임 코드를 계속 실행시키면서 주변 DOS/DPMI 환경만 HLE로 대체하는 것이다. 따라서 마우스 입력 시스템을 먼저 크게 설계하기보다, 관측된 호출에 대해 DOS mouse driver의 최소 응답을 제공하고 다음 실행 흐름을 확인한다.

## 정책

이번 단계에서는 “마우스 드라이버 없음”으로 응답하되, cursor show/hide 계열 호출은 no-op으로 통과시킨다.

`INT 33h AX=0000h`의 일반적인 응답은 다음과 같다.

* `AX=FFFFh`: mouse driver installed
* `AX=0000h`: mouse driver not installed
* `BX`: button count, installed일 때 의미가 있다

`piu_1st`는 아케이드 게임이므로 마우스가 핵심 입력 장치일 가능성이 낮고, 현재 관측 지점도 `spr.res` 실패 경로 이후의 환경 확인 단계로 보인다. 따라서 먼저 `AX=0000h`, `BX=0000h`로 응답하여 guest가 마우스 없음 조건을 자연스럽게 처리하는지 확인한다. 그 뒤 이어지는 `AX=0002h` hide cursor는 실제 cursor state가 아직 없으므로 register를 변경하지 않고 `EIP`만 전진시키는 no-op으로 처리한다.

이 응답이 실행 흐름을 막거나 잘못된 분기로 이어지면, 다음 단계에서 `AX=FFFFh`, `BX=0002h` 같은 “기본 2버튼 마우스 있음” 정책으로 바꾸고 좌표/버튼 상태 HLE를 추가한다.

## 구현 방침

* 일반 HLE dispatch에 `INT 33h` handler를 추가한다.
* trace pre-handler에도 `INT 33h` 감지 함수를 추가한다.
* `AX=0000h`와 `AX=0002h`만 처리한다.
* 처리 시 마지막 interrupt vector/AX 진단 정보를 기록한다.
* `piu_1st` baseline은 새 관측 지점으로 갱신한다.

## 검증

* Win32 x86 host 빌드
* `piu_1st` 단독 실행으로 다음 관측 지점 확인
* `scripts/test_all.ps1`

# INT 33h AX=0000h Mouse Reset HLE Design

## Background

After trace handling for `INT 31h AX=0400h`, `piu_1st` reached `INT 33h`. The first observed register state was `EAX=0x00000000`, which corresponds to the DOS mouse driver reset/status call, `INT 33h AX=0000h`. After handling that, the guest immediately issued `AX=0002h`, the mouse cursor hide call.

The project goal is to keep executing the original 32-bit x86 game code while replacing only the surrounding DOS/DPMI environment with HLE. Therefore, instead of designing a full mouse input system first, this step provides the minimal DOS mouse response for the observed call and checks the next execution flow.

## Policy

This step responds as “mouse driver not installed,” while allowing cursor show/hide style calls to pass as no-ops.

The common `INT 33h AX=0000h` response is:

* `AX=FFFFh`: mouse driver installed
* `AX=0000h`: mouse driver not installed
* `BX`: button count, meaningful when installed

`piu_1st` is an arcade game, so mouse input is unlikely to be a primary input device, and the current observation point appears after the `spr.res` failure path. Therefore, the first policy is to return `AX=0000h`, `BX=0000h` and let the guest handle the no-mouse condition naturally. The following `AX=0002h` hide cursor call has no real cursor state yet, so it is handled as a no-op that only advances `EIP`.

If this blocks execution or sends the guest down an incorrect path, a later step can switch to a basic installed mouse policy such as `AX=FFFFh`, `BX=0002h` and add coordinate/button-state HLE.

## Implementation Policy

* Add an `INT 33h` handler to the general HLE dispatch.
* Add an `INT 33h` detector to the trace pre-handler.
* Handle only `AX=0000h` and `AX=0002h`.
* Record the last interrupt vector/AX diagnostics.
* Update the `piu_1st` baseline to the new observation point.

## Verification

* Build the Win32 x86 host.
* Run `piu_1st` directly and confirm the next observation point.
* Run `scripts/test_all.ps1`.
