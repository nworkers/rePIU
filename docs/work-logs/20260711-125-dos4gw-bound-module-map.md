# DOS4GW 결합 모듈·segment map 복원 작업 로그

## 결과

* MZ 선언 끝 `0xF474`에서 BW overlay가 시작함을 확인했다.
* 공식 `dos16m_exe_header`의 `next_header_pos`로 5개 EXP chain을 복원했다.
* 각 EXP의 header, next header, size, CS:IP, GDT image와 이름을 대조했다.
* DOS4GW.EXP의 selector `0x80..0xA0` GDT entry를 확인했다.
* MZ resident INT21 router가 `AH=FFh`를 service index 0으로 dispatch함을 확인했다.
* router runtime CS가 단일 file base가 아니어서 naive target mapping이 잘못됨을 증명했다.

```mermaid
flowchart LR
    BW["BW chain recovered"] --> GDT["EXP selectors recovered"]
    GDT --> FF["AH=FF -> service 0"]
    FF --> GAP["runtime CS copy/relocation gap"]
    GAP --> CAP["runtime capture recommended"]
```

## 의사결정 지점

다음 단계는 resident loader의 copy/relocation table 전체를 정적으로 복원하거나, 실제 DOS4GW에서 service 0 전후 상태를 캡처하는 것이다. 시간과 정확성 면에서 runtime capture를 권장한다.

## 외부 자료

Open Watcom 공식 `exe16m.h`, `exesigns.h`와 Programming Guide를 사용했으며 분석 문서에 링크를 기록했다.

# DOS4GW Bound-Module and Segment-Map Recovery Work Log

Confirmed the `BW` overlay at the MZ-declared end, recovered all five bound EXP headers through official `next_header_pos` fields, matched their sizes, names, CS:IP and GDT images, identified DOS4GW.EXP selectors `0x80..0xA0`, and confirmed that resident INT 21h maps `AH=FFh` to service zero.

The router's runtime CS is assembled through resident copy/relocation behavior rather than one contiguous file base, so naive target mapping is demonstrably invalid. The next decision is full static recovery of those tables versus runtime capture; runtime capture is recommended for accuracy and time.
