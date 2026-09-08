# Task 638 작업 로그: AOT breakpoint 역매핑 추적

설계: [20260908-638](../design/20260908-638-aot-breakpoint-reverse-map-trace.md)  
작업 지시: [20260908-638](../work-orders/20260908-638-aot-breakpoint-reverse-map-trace.md)

## 한국어

`REPIU_AOT_FAULT_TRACE`가 AOT cache breakpoint도 기록하도록 확장하고,
`REPIU_AOT_FAULT_TRACE_ADDRESS`로 한 cache 주소만 선택할 수 있게 했습니다. 로그는 exact
및 previous guest 역매핑, 두 주소의 breakpoint provenance, cache 끝까지 남은 바이트를
한 줄에 출력합니다. 환경 변수가 없으면 기존 경로와 실행 제어는 바뀌지 않습니다.

Linux x64 실제 `pumpit2a` 실행에서 `0x200829C5`는 exact map이 없었고, 이전 바이트는
guest `0x011C8E0E`로 매핑되었습니다. cache 끝까지 정확히 5바이트가 남았습니다. 코드
대조 결과 이는 마지막 block의 미해결 `kBlockFallthrough`가 만든 5바이트 slot의 첫
`INT3`입니다. 이 작업에서는 복구 정책을 추가하지 않았습니다.

### 검증

* Linux x64 `repiu` 빌드: 통과
* 주소 필터 실제 실행: `exact=0`, `previous=0x011C8E0E`, `tail=5`
* Linux x64 core probe: 통과 (`24/24`)

## English

Extended `REPIU_AOT_FAULT_TRACE` to include AOT-cache breakpoints and added
`REPIU_AOT_FAULT_TRACE_ADDRESS` to select one cache address. Each line reports
exact and previous guest mappings, breakpoint provenance for both addresses,
and distance to the cache end. With no environment variable, execution control
is unchanged.

The real Linux x64 `pumpit2a` run found no exact map at `0x200829C5`; the
preceding byte maps to guest `0x011C8E0E`, with exactly five bytes remaining in
the cache. Source comparison identifies this as the first-byte `INT3` of the
five-byte unresolved `kBlockFallthrough` slot at the final block. This task
does not add a recovery policy.

### Verification

* Linux x64 `repiu` build: passed
* Address-filtered real run: `exact=0`, `previous=0x011C8E0E`, `tail=5`
* Linux x64 core probe: passed (`24/24`)
