# Port I/O 0x02A0 계열 초기화 write 설계

## 배경

`0x02AC/0x10`, `0x02A0/0x1` Port I/O write를 통과시킨 뒤 `piu_1st`는 같은 `OUT DX,EAX` wrapper에서 `port=0x02A2`, `value=0x00000000`에 도달했다.

현재까지 관측된 값은 모두 `0x02A0` 근처의 4-byte OUT이며, 동일한 초기화 흐름 안에 있다. 아직 장치 의미는 확정하지 않는다. 다만 개별 조건식을 계속 늘리는 대신, “관측된 0x02A0 계열 초기화 write”라는 작은 helper로 묶어 관리한다.

## 정책

이번 단계에서는 다음 조합을 관측 기반 allow-list no-op으로 처리한다.

* `port=0x02AC`, `value=0x00000010`
* `port=0x02A0`, `value=0x00000001`
* `port=0x02A2`, `value=0x00000000`

허용 범위는 여전히 정확한 port/value 조합으로 제한한다. `0x02A0` 포트 범위 전체를 열지는 않는다.

## 구현 방침

* Port I/O allow-list 판정을 `IsObservedPortInitializationWrite` helper로 분리한다.
* `0x02A2/0x00000000`을 allow-list에 추가한다.
* 처리 결과는 기존처럼 `ignored`로 기록한다.
* `piu_1st` baseline은 다음 관측 지점으로 갱신한다.

## 검증

* Win32 x86 host 빌드
* `piu_1st` 단독 실행으로 다음 관측 지점 확인
* `scripts/test_all.ps1`

# Port I/O 0x02A0-family Initialization Write Design

## Background

After passing the `0x02AC/0x10` and `0x02A0/0x1` Port I/O writes, `piu_1st` reached `port=0x02A2`, `value=0x00000000` through the same `OUT DX,EAX` wrapper.

All observed values so far are 4-byte OUTs near `0x02A0` and belong to the same initialization flow. This still does not assign device-level meaning. Instead of growing inline conditions, this step groups them under a small “observed 0x02A0-family initialization write” helper.

## Policy

This step handles the following combinations as observation-based allow-listed no-ops.

* `port=0x02AC`, `value=0x00000010`
* `port=0x02A0`, `value=0x00000001`
* `port=0x02A2`, `value=0x00000000`

The allowed scope is still limited to exact port/value combinations. It does not open the whole `0x02A0` port range.

## Implementation Policy

* Move the Port I/O allow-list predicate into `IsObservedPortInitializationWrite`.
* Add `0x02A2/0x00000000` to the allow-list.
* Record the result as `ignored`, same as before.
* Update the `piu_1st` baseline to the next observation point.

## Verification

* Build the Win32 x86 host.
* Run `piu_1st` directly and confirm the next observation point.
* Run `scripts/test_all.ps1`.
