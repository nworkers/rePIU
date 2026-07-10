# Port I/O HLE 라우터 설계

## 배경

`INT 33h AX=0000h/0002h` mouse HLE 처리 이후 `piu_1st`는 privileged instruction 예외 `0xC0000096`으로 중단되었다. 관측된 명령은 `66 EF`이며, 이는 32-bit operand size의 `OUT DX, EAX`이다.

관측값은 다음과 같다.

* `EIP=0x0203505F`
* `EAX=0x00000010`
* `EDX=0x000002AC`
* opcode bytes: `66 EF`

`0x02AC`는 표준 VGA 핵심 포트로 즉시 분류하기 어렵다. 따라서 이번 단계에서는 특정 하드웨어 장치를 구현하지 않고, Port I/O 관측과 좁은 allow-list 기반 no-op 처리를 먼저 추가한다.

## 설계

Port I/O는 DOS interrupt와 달리 하드웨어 또는 DOS extender 내부 포트 접근일 수 있다. 모든 port write를 무조건 통과시키면 필요한 하드웨어 초기화 실패를 숨길 수 있으므로, 관측된 조합만 명시적으로 허용한다.

이번 단계의 정책은 다음과 같다.

* `66 EF` (`OUT DX, EAX`)만 해석한다.
* `port=0x02AC`, `width=4`, `value=0x00000010`만 `ignored` 결과로 처리한다.
* 처리된 Port I/O는 별도 진단 구조체에 기록한다.
* 허용되지 않은 Port I/O는 unsupported로 남겨 다음 작업의 입력으로 삼는다.

## 진단 구조

`Win32PortIoObservation` 구조체를 추가한다.

기록 항목:

* 관측 횟수
* 마지막 명령 주소
* 마지막 opcode
* 방향
* 포트 번호
* 폭
* 값
* 처리 여부
* 결과 문자열

## 검증

* Win32 x86 host 빌드
* `piu_1st` 단독 실행으로 다음 관측 지점 확인
* `scripts/test_all.ps1`

# Port I/O HLE Router Design

## Background

After handling `INT 33h AX=0000h/0002h` mouse HLE, `piu_1st` stopped with privileged instruction exception `0xC0000096`. The observed instruction is `66 EF`, which is `OUT DX, EAX` with 32-bit operand size.

Observed values:

* `EIP=0x0203505F`
* `EAX=0x00000010`
* `EDX=0x000002AC`
* opcode bytes: `66 EF`

`0x02AC` is not immediately recognizable as a core VGA port. Therefore, this step does not implement a specific hardware device. It first adds Port I/O observation and a narrow allow-list based no-op.

## Design

Unlike DOS interrupts, Port I/O can represent hardware access or DOS extender internal port access. Passing every port write as a no-op could hide required hardware initialization failures, so only observed combinations are explicitly allowed.

Policy for this step:

* Decode only `66 EF` (`OUT DX, EAX`).
* Treat only `port=0x02AC`, `width=4`, `value=0x00000010` as handled with result `ignored`.
* Record handled Port I/O in a dedicated diagnostics structure.
* Leave non-allowed Port I/O as unsupported so it can drive the next task.

## Diagnostics Structure

Add `Win32PortIoObservation`.

Recorded fields:

* observed count
* last instruction address
* last opcode
* direction
* port
* width
* value
* handled flag
* result string

## Verification

* Build the Win32 x86 host.
* Run `piu_1st` directly and confirm the next observation point.
* Run `scripts/test_all.ps1`.
