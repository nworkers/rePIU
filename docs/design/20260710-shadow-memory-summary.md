# Shadow memory 요약 진단 설계

## 배경

`piu_1st`는 `spr.res` open 실패 이후 실제 guest writable range 밖으로 많은 store를 수행하며, 현재 HLE는 이를 shadow memory에 기록한다. 마지막 store 하나만으로는 이 쓰기들이 좁은 테이블 초기화인지, 넓은 범위의 메모리 매핑 부족인지 판단하기 어렵다.

## 설계

Win32 trampoline의 shadow memory helper가 다음 요약값을 누적한다.

* shadow write call count
* shadow read hit count
* shadow byte count
* shadow address min/max

`shadow byte count`는 `shadow_memory` map의 unique byte 수를 사용한다. min/max는 shadow write가 발생한 주소 범위를 나타낸다. 이 값들은 guest thread가 멈춘 뒤 attempt로 복사하고 loader에서 출력한다.

## 범위 밖

* shadow memory 내용을 dump하지 않는다.
* 주소별 histogram은 만들지 않는다.
* shadow memory 동작 정책은 변경하지 않는다.

# Shadow Memory Summary Diagnostics Design

## Background

After the `spr.res` open failure, `piu_1st` performs many stores outside the real guest writable range, and the current HLE records them in shadow memory. A single last-store record is not enough to determine whether these writes are a narrow table initialization or evidence of a broader memory mapping gap.

## Design

The Win32 trampoline shadow memory helpers accumulate these summary values.

* shadow write call count
* shadow read hit count
* shadow byte count
* shadow address min/max

`shadow byte count` is the number of unique bytes in the `shadow_memory` map. The min/max values describe the address span touched by shadow writes. These values are copied to the attempt after the guest thread is stopped and printed by the loader.

## Out Of Scope

* Do not dump shadow memory contents.
* Do not add per-address histograms.
* Do not change shadow memory behavior.
