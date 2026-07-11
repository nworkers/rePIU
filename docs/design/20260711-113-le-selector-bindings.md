# DOS4GW LE selector binding 설계

## 확인된 원본 동작

PIU.EXE의 MZ stub은 `DOS4GPATH`에서 `DOS4GW.EXE`를 찾아 DOS `EXEC`로 실행한다. PIU와 함께 배포된 DOS4GW.EXE는 저장소의 Open Watcom 배포본과 SHA-256이 동일하다.

DOS4GW.EXE에 결합된 `LINEXE.EXP`의 object loader는 각 LE object에 대해 다음 DPMI 호출을 수행한다.

```mermaid
flowchart LR
    O["LE object"] --> A["INT 31h AX=0000h<br/>descriptor 1개 할당"]
    A --> B["AX=0007h<br/>base 설정"]
    B --> L["AX=0008h<br/>limit 설정"]
    L --> R["AX=0009h<br/>access rights 설정"]
    R --> F["16:16 fixup에<br/>offset + 할당 selector 기록"]
```

selector는 object index에서 정의되는 고정값이 아니라 DPMI allocator의 반환값이다. PIU 실행에서 관찰된 object 2=`0x24`, object 3=`0x2C`와 순차 할당을 함께 적용하면 이 실행 프로필의 첫 object selector는 `0x1C`이다.

## 구현

* selector allocator는 첫 selector와 다음 할당 위치를 명시적으로 소유한다.
* relocated image는 object 순서대로 descriptor를 하나씩 할당한다.
* selector binding은 할당 selector, target object, relocated base, limit를 보존한다.
* kind `0x03` 16:16 fixup은 source에 target offset 16비트와 실제 할당 selector를 기록한다.
* 실행 초기화는 binding을 `SelectorTable`에 등록한다.
* 관찰된 segment load는 실제 binding을 provisional descriptor로 덮어쓰지 않는다.

현재 첫 selector `0x1C`는 PIU DOS4GW 실행 프로필의 초기 DPMI 상태이다. 다른 실행 프로필을 지원할 때 allocator 초기 상태를 프로필 설정으로 이동한다.

# DOS4GW LE Selector Binding Design

PIU.EXE executes external DOS4GW.EXE through its MZ stub. The bundled DOS4GW binary is byte-identical by SHA-256 to the repository's Open Watcom distribution. Its embedded `LINEXE.EXP` loader dynamically allocates one DPMI descriptor per LE object with INT 31h function 0000h, then sets base, limit, and access rights with functions 0007h, 0008h, and 0009h.

Selectors are allocator results, not fixed values derived from object indices. The PIU profile starts object allocation at selector `0x1C`, as derived from the observed object 2=`0x24` and object 3=`0x2C` values. The runtime allocator assigns descriptors in object order, records their relocated base and limit, writes the allocated selector into 16:16 fixups, and bootstraps the execution selector table.
