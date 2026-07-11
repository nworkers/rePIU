# DOS/4G DLL loader와 INT 21h AX=FF00h 역추적

## 결론

**확인됨:** arena `+0xF3438`의 `INT 3`는 Open Watcom 계열 DLL lazy-loader의 fatal 공통 경로이다. 실제 실행은 세 fatal 조건 중 “DLL loader 초기화 실패”를 선택했다. 직접 원인은 현재 `INT 21h AX=FF00h` HLE가 임시로 `AL=0`을 반환하여 DOS/4G private environment 경로를 비활성화하는 것이다.

## 주소 변환

object 2는 arena `+0x10000`에서 시작한다. 따라서 관찰 주소의 object 내부 오프셋은 다음과 같다.

```text
arena + 0xF3438 - object2_base(+0x10000) = object2 + 0xE3438
```

원본 LE data pages는 파일 오프셋 `0x21400`에서 시작하고 object 2는 두 번째 physical page에서 시작한다. 이 변환으로 얻은 원본 바이트가 실행 로그의 byte window와 일치한다.

## fatal 분기 판별

```mermaid
flowchart TD
    THUNK["DLL import thunk<br/>object2 +0xE33B4"] --> CHECK["call +0xE3F70"]
    CHECK -->|"EAX == 0"| INITERR["EDX = data +0x9623C"]
    CHECK -->|"success"| LOAD["call +0xE3440"]
    LOAD -->|"AX == FFFFh"| LOADERR["EDX = data +0x9626B"]
    LOAD -->|"success"| ENTRY["call +0xE3648"]
    ENTRY -->|"returned CS != CS"| ENTRYERR["EDX = data +0x9628D"]
    INITERR --> INT3["object2 +0xE3438 INT 3"]
    LOADERR --> INT3
    ENTRYERR --> INT3
```

| 조건 | relocation 전 `EDX` | 원본 문자열 |
| --- | --- | --- |
| 초기화 실패 | `0x0009623C` | `Fatal error: unable to initialize DLL loader.` |
| DLL load 실패 | `0x0009626B` | `Fatal error: unable to load DLL.` |
| entry point 검색 실패 | `0x0009628D` | `Fatal error: unable to find entry point in DLL.` |

재실행의 예외 레지스터는 `EDX=0x021A623C`, `EAX=0`이었다. `0x021A623C`는 첫 문자열의 relocated 주소이므로 첫 조건이 확정된다. selector 비교나 DLL 파일 load가 현재 직접 원인은 아니다.

## 초기화 함수 역추적

`object2+0xE3F70`은 세 selector 전역값이 모두 비어 있지 않으면 즉시 성공한다. 초기 상태에서는 다음 순서로 초기화를 시도한다.

1. `object2+0xE37E8`에서 DOS/4G private environment/module 정보를 찾는다.
2. `object2+0xE39B4`에서 관련 table을 검사한다.
3. `object2+0xE3DEC`을 세 번 호출해 필요한 descriptor를 만든다.
4. 세 selector 중 하나라도 0이면 만든 descriptor를 해제하고 0을 반환한다.

첫 단계 `+0xE37E8`은 전역 word `data+0x96354`가 0이면 즉시 실패한다. 이 전역을 설정하는 시작 코드는 `object2+0xE4CEC`에 있다.

```mermaid
sequenceDiagram
    participant P as PIU startup
    participant H as rePIU INT21 HLE
    participant D as DLL loader
    P->>H: AX=FF00h, DX=0078h
    H-->>P: AL=0, CF=0 (current temporary policy)
    P->>P: take legacy fallback
    Note over P: GS selector is not saved to data+0x96354
    D->>D: read data+0x96354 == 0
    D-->>D: return EAX=0
    D->>D: INT 3 with initialization fatal message
```

원본 시작 코드는 `AX=FF00h` 뒤 `AL`을 검사한다. `AL != 0`이면 `GS`가 0인지 확인하고, 0이 아니면 `GS`를 `data+0x96354`에 저장한다. 현재 HLE는 `AL=0`과 carry clear만 제공하므로 이 저장 경로에 들어가지 않는다.

## 확정된 것과 남은 것

**확정됨**

* 새 `INT 3`는 DLL loader 초기화 fatal 경로이다.
* 현재 임시 `AX=FF00h -> AL=0` 응답이 해당 경로의 직접 원인이다.
* 단순히 `INT 3`를 skip하거나 selector 비교를 완화하는 것은 올바른 수정이 아니다.

**미확정**

* 성공 시 `AL`의 정확한 bit 의미와 값.
* `GS`가 가리켜야 하는 DOS/4G private structure의 전체 layout.
* `GS:0x42`에서 시작하는 far-pointer/module chain과 PIU가 찾는 table의 최소 필드 집합.

따라서 다음 수정은 `AL`만 1로 바꾸는 방식으로 진행하면 안 된다. 유효한 selector와 `GS:0x42` 구조가 함께 없으면 더 뒤에서 잘못된 memory를 읽는다. 다음 결정은 DOS4GW 원본/동일 바이너리를 추가 역분석하여 private structure를 최소 HLE로 모델링할지, 실제 DOS4GW 실행에서 반환 상태와 구조를 캡처할지이다.

# DOS/4G DLL Loader and INT 21h AX=FF00h Provenance

## DOS/32A 교차 확인과 적용 한계

[DOS/32A 공식 `INT 21h` 구현](https://github.com/amindlost/dos32a/blob/master/src/dos32a/text/client/int21h.asm)은 DOS/4G identification call을 `AX=FF00h`, `DX=0078h`로 판별하고 `EAX=FFFF3447h`, `GS=client data selector`를 반환한다. [공식 프로그래머 문서](https://github.com/amindlost/dos32a/blob/master/docs/html/prog/int21h/0ff00.htm)도 이 signature를 확인한다. 이는 기존에 미확정이었던 성공 `EAX` 값과 `GS`의 역할을 독립적으로 교차 확인한다.

다만 DOS/32A 저장소 전체에서 PIU가 검색하는 `LINEXE_LOADER` 및 네 `LINEXE_*` export의 근거는 발견되지 않았다. 따라서 DOS/32A의 drop-in replacement 성격만으로 `GS:0x42` 이후 private layout이 DOS/4GW와 동일하다고 간주하지 않는다. rePIU는 DOS/32A 코드를 사용하지 않고 register contract만 참고하며, private module chain은 PIU consumer와 DOS/4GW 바이너리 증거로 복원한다.

후속 [DOS4GW 원본 frame/data-flow 분석](dos4g-service-zero-frame-dataflow.md)에서 동일 입력의 원본 반환을 `AX=FFFFh`, CF=1, GS preserved로 확정했다. DOS/32A의 `EAX=FFFF3447h`, explicit GS assignment는 consumer-visible compatibility를 제공하는 다른 구현 방식이다.

```mermaid
sequenceDiagram
    participant P as PIU
    participant H as rePIU HLE
    participant E as private environment
    P->>H: AX=FF00h, DX=0078h
    H->>E: 유효한 GS:0x42 구조 확인
    alt 구조 준비 완료
        H-->>P: EAX=FFFF3447h, GS=selector
    else 구조 미완성
        H-->>P: unsupported/fallback
    end
```

## Conclusion

**Confirmed:** The `INT 3` at arena `+0xF3438` is the common fatal path of an Open Watcom-style DLL lazy loader. Runtime `EDX=0x021A623C` selects the first of three messages: `Fatal error: unable to initialize DLL loader.` The other branches represent DLL-load and entry-point lookup failures.

The direct cause is the current temporary `INT 21h AX=FF00h` response. Startup checks `AL` after the call. When `AL` is nonzero and `GS` is valid, it stores `GS` in the global later consumed by the DLL initializer. rePIU currently returns `AL=0`, so startup takes the legacy fallback and never records that selector. The initializer reads zero and fails before loading a DLL.

Changing only `AL` is unsafe: the original code subsequently dereferences a DOS/4G private structure beginning through `GS:0x42`. The exact success flags, selector, structure layout, and module-chain fields remain unresolved. The next faithful implementation must derive and model that contract or capture it from an actual DOS4GW execution; the breakpoint must not be skipped.

## GS:0x42 최소 field map

후속 [provider-side private environment 분석](dos4g-client-gs-private-environment.md)에서 원본 값을 확정했다. GS는 `0x20`, root는 `0020:0042 -> 0090:059A`, module은 `LINEXE_LOADER`, export table은 `0090:0522`이며 15개 entry를 가진다.

PIU consumer `object2+0xE37E8`의 load width와 offset으로 다음 구조를 확인했다. 모든 pointer는 16-bit offset과 16-bit selector로 구성된 far pointer이다.

```mermaid
classDiagram
    class PrivateEnvironment {
        +0x42 first_module_offset: word
        +0x44 first_module_selector: word
    }
    class ModuleRecord {
        +0x00 next_offset: word
        +0x02 next_selector: word
        +0x04 name_offset: word
        +0x06 name_selector: word
        +0x10 export_count: word
        +0x12 export_table_offset: word
        +0x14 export_table_selector: word
    }
    class ExportEntry {
        +0x00 name_offset: word
        +0x02 name_selector: word
        +0x04 value: dword
    }
    PrivateEnvironment --> ModuleRecord
    ModuleRecord --> ModuleRecord : next
    ModuleRecord --> ExportEntry : count × 8 bytes
```

module 이름은 `LINEXE_LOADER`와 비교된다. 일치한 module의 8-byte export entry를 순회하며 `LINEXE_LOADMODULE`, `LINEXE_FREEMODULE`, `GETLOADTABLE`, `GETLOADNAME` 네 이름을 찾는다.

`AX=FF00h` 성공 여부는 `AL != 0`으로 검사되고, 성공 시 `GS`가 private environment selector로 저장된다. 따라서 최소 반환 형태는 **nonzero AL + valid GS selector**임이 consumer 측에서 확인된다. 정확한 성공 `AL` 값은 provider handler를 완전히 복원하기 전까지 inferred 상태로 둔다.

DOS4GW의 16-bit INT 21h router에서는 `AH=FFh`가 명시적으로 special dispatch에 포함되고 `inc AH` 뒤 service index 0으로 변환되는 것까지 확인했다. 다만 결합 모듈의 logical IP와 file offset mapping이 여러 segment에 걸쳐 있어 service 0 target의 전체 provider 반환 코드는 아직 확정하지 못했다.

## fatal tail 실행 확인

제한된 signature의 `INT 3`를 진단 후 재개하자 원본 error printer가 실제로 `Fatal error: unable to initialize DLL loader.`를 console에 출력했다.

출력 경로에서 추가로 `INT 21h AH=09h`, 양방향 low-memory register-frame copy, `INT 31h AX=0300h/BL=2Fh`, 마지막 `INT 21h AX=4C01h`이 관찰됐다. 따라서 source에 있는 `HLT`는 DOS terminate가 반환했을 때를 위한 최종 fallback이며 정상 fatal 경로에서는 `AX=4C01h`이 먼저 실행된다.

## Minimum GS:0x42 field map and fatal-tail confirmation

PIU's consumer confirms a far pointer at `GS:0x42` to a linked module record. The record contains next and name far pointers at offsets `0x00` and `0x04`, an export count at `0x10`, and an export-table far pointer at `0x12`. Each export entry is eight bytes: a name far pointer followed by a dword value. PIU locates `LINEXE_LOADER` and resolves `LINEXE_LOADMODULE`, `LINEXE_FREEMODULE`, `GETLOADTABLE`, and `GETLOADNAME`.

## 공용 segment load 실행 관찰 / Shared segment-load observation

공용 segment override decoder 적용 후 root pointer와 `LINEXE_LOADER` module 이름 비교는 통과했다. 합성 레코드의 export count/table도 `8`, `0090:0522`로 직접 확인됐다. 현재 실패 경계는 module 후보에서 유효했던 stack-local pointer `0090:059A`가 export count 비교 전에 `0000:0000`으로 사라지는 구간이다. 따라서 private environment 레이아웃 문제가 아니라 module match 뒤 stack-local 보존 명령의 실행 의미가 다음 분석 대상이다.

After enabling the shared segment-override decoder, root-pointer resolution and the `LINEXE_LOADER` module-name comparison succeed. Direct inspection also confirms export count/table values of `8` and `0090:0522`. The remaining failure boundary is the stack-local module pointer changing from `0090:059A` at the candidate to `0000:0000` before the export-count comparison, making stack-local instruction semantics the next target rather than the private-environment layout.

후속 계측에서 pointer 자체는 jump target에서도 정상임을 확인했다. 0 관찰값은 HLE가 EIP를 민감 명령으로 옮긴 뒤 host segment로 native 실행한 결과였다. 후속 segment load/store drain과 `8C /r` memory-form guest selector 저장을 구현한 뒤 export 8개가 모두 resolve됐다. 마지막 관찰값은 `0090:055E -> 0080:0138`이다. 현재 실패 경계는 export 구조가 아니라 resolve된 `0080:xxxx` call gate 호출이다.

Later observation proved that the pointer remained valid at the jump target. The zero arose when HLE advanced EIP and native execution used a host segment. Draining following sensitive loads/stores and implementing guest-selector `8C /r` memory stores resolves all eight exports; the last observed mapping is `0090:055E -> 0080:0138`. The active boundary is now call-gate invocation rather than export structure recovery.

The consumer proves that success requires nonzero `AL` and a valid private-environment selector in `GS`. DOS/32A supplies this as `EAX=0xFFFF3447` plus explicit GS assignment. Original DOS4GW returns low `AX=FFFFh`, carry set, and preserved `GS=0x20`. Provider-side recovery establishes `0020:0042 -> 0090:059A`, the `LINEXE_LOADER` record and its 15-entry export table at `0090:0522`.

Continuing only the recognized fatal-tail breakpoint caused the original error printer to emit the fatal message. The path then used DOS `AH=09h`, low-memory register-frame copies, DPMI `AX=0300h/BL=2Fh`, and finally DOS terminate `AX=4C01h`. The following `HLT` is a fallback if termination returns.
