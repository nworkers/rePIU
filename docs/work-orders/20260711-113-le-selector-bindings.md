# DOS4GW LE selector binding 작업 지시

```mermaid
flowchart LR
    S["PIU MZ stub 확인"] --> D["동일 DOS4GW 식별"]
    D --> L["LINEXE loader 역분석"]
    L --> A["DPMI selector allocator 구현"]
    A --> F["16:16 fixup 적용"]
    F --> V["분석기·빌드·실행 검증"]
```

1. PIU MZ stub과 외부 DOS4GW 의존성을 확인한다.
2. 배포 DOS4GW 바이너리의 동일성을 해시로 검증한다.
3. `LINEXE.EXP`의 descriptor 할당과 LE fixup 경로를 역분석한다.
4. 고정 selector 계산을 DPMI 방식 allocator로 대체한다.
5. object descriptor binding과 16:16 fixup 기록을 구현한다.
6. analyzer 진단, 빌드, 실제 PIU 실행을 검증한다.
7. 분석 문서와 작업 로그를 갱신하고 커밋한다.

# DOS4GW LE Selector Binding Work Order

Verify the PIU MZ stub and external DOS4GW dependency, identify the exact DOS4GW binary, reverse the `LINEXE.EXP` descriptor and fixup paths, replace fixed selector arithmetic with a DPMI-style allocator, apply 16:16 fixups, validate diagnostics/build/runtime behavior, update documentation, and commit the task.
