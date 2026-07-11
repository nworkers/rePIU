# Runtime arena, shadow memory, sentinel 분석

```mermaid
flowchart TD
    ACCESS["Guest Memory Access"] --> REAL{"Inside Runtime Arena?"}
    REAL -->|yes| ARENA["Read / Write Real Memory"]
    REAL -->|no| SHADOW{"Complete Shadow Value?"}
    SHADOW -->|yes| MAP["Read / Write Shadow Memory"]
    SHADOW -->|no| ZERO{"guest DS + first 4 KiB read?"}
    ZERO -->|yes| ZPAGE["Zero-backed DOS Page"]
    ZERO -->|no| FAULT["Keep Fault Visible"]
    SENTINEL["Allocator Sentinel"] --> MAP
    BOUNDARY["Boundary Object Chain"] --> MAP
```

## Runtime arena

**확인됨:** LE image, guest stack, heap 성격의 확장 영역을 하나의 Win32 precommitted arena에 배치한다. DOS resize 관찰에 따라 slack을 확장하여 정상적인 guest write가 실제 memory에 들어가도록 했다.

## Shadow memory

**확인됨:** arena 밖이지만 원본 allocator가 유효하다고 간주하는 주소의 store를 byte-addressed map에 보존한다. 이후 `8B /r` 등은 4바이트가 모두 존재할 때만 shadow value를 읽는다. 이는 실제 memory를 무제한 확장하는 대신 분석된 범위만 보존하는 안전망이다.

## Sentinel과 allocator metadata

**확인됨:** allocator 실패/경계 표식으로 보이는 `0xFFFFFFFF` store와 그 주변 block header field가 연속적으로 관찰되었다. sentinel address와 앞선 header address의 차이가 block size register와 일치했고, 인접 `89 /r` store들이 linked allocator metadata 형태를 만들었다.

**추정:** `0xFFFFFFFF`는 일반 데이터가 아니라 allocator 목록의 종료 또는 실패 상태를 나타내는 표식이다. 정확한 원본 allocator 자료구조 이름은 아직 확정하지 않았다.

## Arena 경계 객체 chain

**확인됨:** arena 마지막 64바이트에서 시작한 객체의 field store가 경계를 넘는다. 다음 object base가 직전 frontier와 정확히 일치할 때만 chain을 연장한다. 관측된 `ESI=0x640`, `EDX=0x2C`에서 span `0x11300`을 계산했고 `66 C7`, `C7`, `89`, `D9` store를 제한적으로 shadow 처리했다.

## DS zero page

**확인됨:** `8B 16`과 `ESI=0`은 host null pointer가 아니라 guest `DS:0` 접근이다. relocated base를 더하지 않고, guest `DS`가 활성화된 unprefixed `8B /r`의 첫 4 KiB miss만 zero-backed DOS low memory로 처리한다. `0xFF000000` 같은 고주소는 계속 거부한다.

## Shadow arithmetic source

**확인됨:** `03 07` (`add eax,[edi]`)이 allocator metadata shadow dword를 source로 읽는다. source 전체가 shadow에 있을 때만 ADD를 수행하고 destination register와 여섯 산술 flag를 복원한다. 다음 명령 `83 0E 01`은 같은 metadata의 bit 0을 설정하는 read-modify-write로 관찰되었다.

**확인됨:** `83 0E 01`의 OR 결과를 같은 shadow dword에 기록한 뒤 allocator 호출이 반환했고, 다음 field byte를 읽는 `38 10` (`cmp [eax],dl`)까지 진행했다. 이는 metadata가 단순 write-only 진단 값이 아니라 원본 allocator control flow에서 다시 읽히는 실제 자료구조임을 강화한다.

**확인됨:** 첫 shadow byte CMP 뒤 같은 block offset `+0x20`의 unwritten byte compare가 관찰되었다. allocator probe에서 확인된 요청 크기 `0x2C`와 `0x1008`만 pending 상태로 보존하고 header OR에서 block base와 결합했다. `[block+4, block+size-4)` 안의 unwritten byte만 0으로 읽고 explicit shadow store를 우선하자 `38 50 20`을 통과해 파일 파싱 루프와 quiet timeout까지 진행했다.

```mermaid
flowchart LR
    P["Probe: size 0x2C or 0x1008"] --> H["Header OR confirms block B"]
    H --> Z["Zero payload: B+4 .. B+size-4"]
    Z --> C["Unwritten CMP reads 0"]
    W["Explicit shadow write"] --> C
    U["Unknown size / outside range"] --> F["Keep fault"]
```

**안전성 확인:** 단순히 `8 <= EAX <= 1 MiB`를 허용하면 allocator와 무관한 값을 크기로 오인해 Windows heap corruption `0xC0000374`가 발생했다. 확인된 두 크기의 allowlist로 제한한 뒤 전체 테스트가 통과했고 손상이 재현되지 않았다.

# Runtime Arena, Shadow Memory, and Sentinel Analysis

The runtime arena contains the relocated LE image, guest stack, and observed heap expansion. Byte-addressed shadow memory preserves only analyzed out-of-arena stores and serves reads only when every requested byte exists.

Observed `0xFFFFFFFF` stores and adjacent block-header writes form an allocator sentinel/metadata pattern, although the original allocator structure name remains inferred. Boundary objects are chained only through exact base/frontier continuity and a validated span. Guest `DS:0` is treated as low memory, never as `relocated_base + 0`.
