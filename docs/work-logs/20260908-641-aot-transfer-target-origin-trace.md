# Task 641 작업 로그: AOT 전이 target 출처 추적

설계: [20260908-641](../design/20260908-641-aot-transfer-target-origin-trace.md)  
작업 지시: [20260908-641](../work-orders/20260908-641-aot-transfer-target-origin-trace.md)  
분석: [linux-port-frontier 3.78](../analysis/linux-port-frontier.md)

## 한국어

`REPIU_AOT_TRANSFER_TARGET_TRACE`를 추가했습니다. 공용 간접 CALL/JMP와 RET handler는
source, target, 명령 bytes, 레지스터와 기대 반환 정보를 출력하고, Linux x64 전용
thunk 경로는 producer 종류·주소, 원본 bytes, 소비 stack 슬롯을 출력합니다. 주소
필터가 없으면 출력과 실행은 기존과 같습니다.

실제 `pumpit2a`에서 `0x011A8E10`은 Linux x64 thunk가 처리한 guest `0x010F1E56`의
`RET(C3)` target으로 확인됐습니다. 소비 슬롯 `0x0158CC84`에는 이미
`0x011A8E10`이 들어 있었습니다. 최초 stack-writer 보조 캡처의 sequence 0은 필수
활성화 옵션을 빠뜨린 결과였으며, Task 642가 이 예비 해석을 정정하고 대체합니다.

### 검증

* Linux x64 `repiu`와 `repiu_core_probe` 빌드: 통과
* Linux x64 core probe: `24/24`, failures `0`
* 실제 filtered capture: `0x010F1E56 RET -> [0x0158CC84] -> 0x011A8E10`
* 이후 frontier: 기존과 동일한 cache `0x20328014` address-zero fault

게임은 아직 정상 실행되지 않습니다. Task 642에서 stack 슬롯 `0x0158CC84`의 실제
writer를 이어서 확인합니다.

## English

Added `REPIU_AOT_TRANSFER_TARGET_TRACE`. Shared indirect CALL/JMP and RET
handlers report source, target, instruction bytes, registers, and expected
return state. The Linux x64 dedicated thunk path reports producer kind and
address, original bytes, and the consumed stack slot. With no filter, output
and execution remain unchanged.

In real `pumpit2a`, `0x011A8E10` is the target of the guest `RET` (`C3`) at
`0x010F1E56`, handled through the Linux x64 thunk. Consumed slot `0x0158CC84`
already held `0x011A8E10`. The first stack-writer companion capture reported
zero sequence because its required enable setting was omitted; Task 642
corrects and supersedes that preliminary interpretation.

### Verification

* Linux x64 `repiu` and `repiu_core_probe` build: passed
* Linux x64 core probe: `24/24`, failures `0`
* Real filtered capture: `0x010F1E56 RET -> [0x0158CC84] -> 0x011A8E10`
* Subsequent frontier: unchanged cache `0x20328014` address-zero fault

The game still does not run normally. Task 642 follows the actual writer of
stack slot `0x0158CC84`.
