# Port I/O 0x02A0 allow-list 확장 설계

## 배경

Port I/O HLE 라우터 추가 후 `piu_1st`는 첫 관측값인 `OUT DX,EAX port=0x02AC value=0x00000010`을 통과했다. 다음 관측 지점은 같은 `66 EF` (`OUT DX,EAX`) 명령이며, 레지스터 값은 다음과 같다.

* `EIP=0x020F5726`
* `EAX=0x00000001`
* `EDX=0x000002A0`
* opcode bytes: `66 EF`

해당 위치 주변 바이트는 작은 port I/O wrapper처럼 보이며, 직전 `0x02AC` write와 같은 초기화 흐름에 속한다. 아직 `0x02A0`의 장치 의미를 확정할 근거는 부족하다.

## 정책

이번 단계에서는 `port=0x02A0`, `value=0x00000001`, `width=4` 조합만 추가 allow-list no-op으로 처리한다.

모든 Port I/O를 통과시키지 않고 관측된 조합만 허용하는 기존 정책은 유지한다. 다음 Port I/O가 관측되면 다시 unsupported로 남겨서 장치 의미를 누적 판단한다.

## 구현 방침

* Port I/O allow-list 조건에 `0x02A0/0x00000001/4-byte OUT`을 추가한다.
* 처리 결과는 기존과 같이 `ignored`로 기록한다.
* `piu_1st` baseline은 다음 관측 지점으로 갱신한다.

## 검증

* Win32 x86 host 빌드
* `piu_1st` 단독 실행으로 다음 관측 지점 확인
* `scripts/test_all.ps1`

# Port I/O 0x02A0 Allow-list Extension Design

## Background

After adding the Port I/O HLE router, `piu_1st` passed the first observed write, `OUT DX,EAX port=0x02AC value=0x00000010`. The next observation point is the same `66 EF` (`OUT DX,EAX`) instruction with the following register values.

* `EIP=0x020F5726`
* `EAX=0x00000001`
* `EDX=0x000002A0`
* opcode bytes: `66 EF`

The surrounding bytes look like a small Port I/O wrapper, and this write belongs to the same initialization flow as the previous `0x02AC` write. There is not enough evidence yet to assign a specific device meaning to `0x02A0`.

## Policy

This step adds only the `port=0x02A0`, `value=0x00000001`, `width=4` combination as an allow-listed no-op.

The existing policy remains: do not pass every Port I/O through, only observed combinations. The next Port I/O will remain unsupported and become the next input for device-meaning analysis.

## Implementation Policy

* Add `0x02A0/0x00000001/4-byte OUT` to the Port I/O allow-list.
* Record the result as `ignored`, same as the existing allowed case.
* Update the `piu_1st` baseline to the next observation point.

## Verification

* Build the Win32 x86 host.
* Run `piu_1st` directly and confirm the next observation point.
* Run `scripts/test_all.ps1`.
