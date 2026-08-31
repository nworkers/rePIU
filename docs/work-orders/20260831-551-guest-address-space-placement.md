# 20260831-551 guest 주소 공간 배치 측정 작업 지시서

## 한국어

### 목적

Task 546 결정 4의 "하위 4 GiB 배치" 항목을 논증이 아니라 **측정**으로 정리합니다.
결정 4는 이 정책을 둘로 쪼개라고 적었습니다.

1. guest address 보존을 위해 **진짜 필요한** 부분
2. host pointer가 4 GiB 아래 있으리라는 **우연한 가정**

두 항목의 답이 다르고, 어느 쪽도 코드를 읽는 것만으로는 확정되지 않습니다. x86-64
Linux 프로세스가 guest arena를 그 32비트 base에 실제로 줄 수 있는지는 커널이
정하는 문제이며, 거절당하면 느린 경로가 아니라 port의 끝입니다.

### 작업

- `guest_address_space` probe를 추가한다.
- 예약 요청 값은 `GetBuiltInTargetProfiles()`의 `runtime_reservation_hint`에서
  읽는다. 상수를 복제하지 않는다 — host가 쓰는 값과 어긋날 수 없어야 한다.
- **계약**을 검사한다: guest base에 대한 예약은 정확히 그 주소로 오거나 아예 오지
  않아야 하고, 다른 주소가 조용히 받아들여지면 안 된다.
- **측정**을 보고한다: 정확한 배치가 성공했는지 여부는 host마다 다르므로 pass/fail이
  아니라 값으로 남긴다.
- `vm.mmap_min_addr`과 가장 낮은 guest base 사이의 여유를 보고한다.
- host pointer가 4 GiB 위에 있는지 항목별로 보고한다: engine 자신의 code, stack,
  heap, 그리고 `/proc/self/maps`가 보여주는 실행 가능한 매핑.

### 검증

Linux x64, Linux i386, Win32에서 빌드·실행합니다. 32비트 host에서는 모든 항목이
4 GiB 아래여야 하며, 그것이 x64 결과를 읽는 기준선이 됩니다.

## English

### Objective

Settle the "place below 4 GiB" item of Task 546's decision 4 by **measurement**
rather than argument. Decision 4 asks for the policy to be split in two:

1. what is **genuinely required** to preserve guest addresses
2. the **accidental assumption** that host pointers sit below 4 GiB

The two have different answers, and neither is settled by reading code. Whether an
x86-64 Linux process can actually be given the guest arena at its 32-bit base is the
kernel's to decide, and a refusal is the end of the port rather than a slow path.

### Work items

- Add a `guest_address_space` probe.
- Take the reservation requests from `GetBuiltInTargetProfiles()`'s
  `runtime_reservation_hint` rather than copying constants, so the numbers cannot
  drift from the ones the host uses.
- Check the **contract**: a reservation at the guest base must land exactly there or
  not count, and a different address must never be quietly accepted.
- Report the **measurement**: whether exact placement succeeded differs by host, so it
  is a value rather than a pass or a failure.
- Report the headroom between `vm.mmap_min_addr` and the lowest guest base.
- Report, item by item, which host pointers live above 4 GiB: the engine's own code,
  the stack, the heap, and the executable mappings `/proc/self/maps` shows.

### Verification

Build and run on Linux x64, Linux i386, and Win32. On a 32-bit host every item must be
below 4 GiB, which is the baseline the x64 numbers are read against.
