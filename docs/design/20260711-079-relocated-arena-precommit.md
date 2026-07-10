# relocated arena 선점 예약 설계

## 배경

`piu_1st` 실행에서 relocated base 후보를 `VirtualQuery`로 확인한 뒤, 나중에 `VirtualAlloc(MEM_RESERVE | MEM_COMMIT)`으로 실제 배치를 시도할 때 error `487`이 발생할 수 있다.

현재 흐름은 후보 주소가 비어 있는지 관측만 한 뒤 여러 준비 단계를 거쳐 실제 arena를 확보한다. 이 사이에 host process의 다른 allocation이 같은 주소 범위를 차지하면, probe 결과는 `available`이었더라도 실제 배치는 실패한다.

## 설계

후보 주소 선택은 `VirtualQuery` 결과가 아니라 실제 `VirtualAlloc` 성공으로 확정한다.

* relocated arena 후보를 순회하면서 `MEM_RESERVE | MEM_COMMIT`으로 즉시 확보한다.
* 확보에 성공한 arena base를 기준으로 relocated image plan을 만든다.
* image placement 단계에서는 다시 `VirtualAlloc`하지 않고, 이미 확보된 arena에 object bytes를 복사하고 protection만 적용한다.
* relocation plan 생성이나 object 복사/protection이 실패하면 확보한 arena를 해제하고 다음 후보를 시도할 수 있는 구조를 유지한다.

이번 단계에서는 retry loop 전체를 크게 재구성하지 않고, 선택 단계에서 선점 예약을 끝낸 뒤 기존 실행 경로에 넘긴다.

## 기대 결과

`VirtualQuery`와 `VirtualAlloc` 사이의 주소 점유 race를 제거하여 `piu_1st`가 안정적으로 현재 guest 실행 blocker까지 도달해야 한다.

## 범위 밖

* DOS memory block chain/MCB 구현은 하지 않는다.
* relocated base 후보 목록 정책은 바꾸지 않는다.
* `0x02A0/0x5` Port I/O 의미 해석은 이번 작업에서 처리하지 않는다.

# Relocated Arena Precommit Design

## Background

During `piu_1st` execution, relocated base selection can report a candidate as available through `VirtualQuery`, but the later `VirtualAlloc(MEM_RESERVE | MEM_COMMIT)` placement can still fail with error `487`.

The current flow only observes whether a candidate address range is free, performs several preparation steps, and then reserves the real arena. If another host-process allocation occupies the same range in between, placement fails even though the probe result was `available`.

## Design

Candidate selection should be decided by actual `VirtualAlloc` success, not by `VirtualQuery` alone.

* Iterate relocated arena candidates and immediately reserve/commit each candidate with `MEM_RESERVE | MEM_COMMIT`.
* Build the relocated image plan from the arena base that was successfully acquired.
* During image placement, do not call `VirtualAlloc` again. Copy object bytes into the already acquired arena and apply object protection.
* If relocation planning or object copy/protection fails, keep the structure able to release the acquired arena and try another candidate later.

This step avoids a broad retry-loop rewrite. It precommits the arena during selection and passes that acquired arena into the existing execution path.

## Expected Result

Removing the address-occupation race between `VirtualQuery` and `VirtualAlloc` should let `piu_1st` consistently reach the current guest execution blocker.

## Out Of Scope

* Do not implement DOS memory block chains/MCBs.
* Do not change the relocated base candidate list policy.
* Do not interpret or handle the `0x02A0/0x5` Port I/O meaning in this task.
