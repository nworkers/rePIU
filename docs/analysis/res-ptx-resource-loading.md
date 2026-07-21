# RES/PTX resource loading 분석 / RES/PTX Resource Loading Analysis

## 확인된 실행 흐름

```mermaid
flowchart LR
    D[PIU.DAT RES archive] --> H[header/table 0x2A00 read]
    H --> P[payload 0x5C00 read]
    P --> B[archive buffer base 0x0393B650]
    B --> F[HFONT1 pointer 0x03BB6AE9]
    F --> Z[zero-filled header]
    Z --> E[Not PTX file -> exit -1]
```

**확인됨:** `Not PTX file`은 object 2 `+0xDDC98`의 memory PTX loader에서 발생한다. loader는 입력 16바이트를 stack에 복사한 뒤 `PTX\0` magic과 version word `0x0100`을 검사한다. error printer는 반환하지 않고 Watcom `exit(-1)`로 연결된다. 종료 stack의 return address는 `+0xDDD2B`, runtime error path `+0xDF884`, runtime cleanup `+0xE52D8`이다.

입력 pointer는 `0x03BB6AE9`, caller는 `+0xE1DC9`이다. 상위 caller의 문자열은 `hfont1.tga`, `hfont2.tga`이며 `PIU.DAT` table에는 대응하는 `HFONT1.PTX`, `HFONT2.PTX`가 있다. 두 entry의 실제 payload는 각각 absolute `0x27B499`, `0x28AE4B`에서 정상 `PTX\0` header를 가진다.

archive buffer base는 pointer와 file offset으로부터 `0x0393B650`으로 역산된다. `0x0393B650 + 0x27B499 = 0x03BB6AE9`이므로 entry pointer 계산은 정확하다. 그러나 file-I/O ring은 archive가 `0x8600`까지만 읽혔음을 보여 주며 pointer 위치는 zero-filled reserve에 남는다.

`PIU.DAT` header의 payload size는 `0x00855C29`이지만 실제 payload read는 `0x5C00`이다. table/header `0x2A00`을 더하면 최종 file position `0x8600`이 된다. 이는 payload size의 low 16-bit `0x5C29`만 read loop에 전달된 패턴과 일치한다.

## 미확정

상위 16비트가 사라지는 정확한 명령과 원인은 아직 미확정이다. 후보는 RES loader 내부의 size 전달, Watcom read wrapper ABI, 또는 관련 guest instruction HLE이다. 다음 단계는 `0x00855C29` consumer에서 DOS read loop까지 size provenance를 정적으로 복원하는 것이다.

## Confirmed execution flow

**Confirmed:** `Not PTX file` comes from the memory PTX loader at object 2 `+0xDDC98`. It copies 16 input bytes, checks `PTX\0` and version `0x0100`, and calls a non-returning error printer that reaches Watcom `exit(-1)`. Termination-stack return addresses are `+0xDDD2B`, runtime error path `+0xDF884`, and runtime cleanup `+0xE52D8`.

The input pointer is `0x03BB6AE9`, called from `+0xE1DC9`. Higher frames reference `hfont1.tga` and `hfont2.tga`; the archive contains `HFONT1.PTX` and `HFONT2.PTX` with valid headers at absolute file offsets `0x27B499` and `0x28AE4B`.

The archive buffer base is `0x0393B650`, and `base + 0x27B499` equals the observed input pointer, proving entry-pointer arithmetic is correct. Earlier runs read only through `0x8600`, leaving the distant entry zero-filled.

## 32-bit DOS/4GW read ABI 복원 / Restored 32-bit DOS/4GW Read ABI

**확인됨:** read #26의 `INT 21h AH=3Fh` 진입 EIP는 `0x030F87B7`이고, 호출 반환 주소는 `0x030F53F3`입니다. 진입 스택에는 요청 후보 `0x00854D00`, 전체 payload 크기 `0x00855C29`, 목적지 `0x0393F050`이 동시에 남아 있었습니다. 따라서 RES parser나 Watcom 상위 호출부에서 크기가 16비트로 잘린 것이 아닙니다.

원본 wrapper는 `mov ecx, ebx; mov ah, 3fh; int 21h`를 실행하고 이후 32-bit `EAX`를 검사합니다. 상위 loop도 반환된 `EAX`를 32-bit remaining count에서 뺍니다. HLE만 `ECX & 0xffff`와 16-bit `AX` 반환을 사용하고 있었으므로, 첫 큰 요청을 `0x4D00`으로 축소하고 다음 반복을 0-byte read로 만들었습니다.

```mermaid
flowchart LR
    SIZE[RES payload size<br/>0x00855C29] --> LOOP[Watcom 32-bit read loop]
    LOOP --> REQ[EBX/ECX<br/>0x00854D00]
    REQ --> INT[INT 21h AH=3Fh]
    INT -->|잘못된 HLE: CX| SHORT[0x4D00 only]
    INT -->|복원된 HLE: ECX/EAX| FULL[full payload read]
    SHORT --> ZERO[unread PTX memory is zero]
    FULL --> NEXT[PTX error path passed]
```

**검증됨:** `ECX` 전체를 요청 크기로 사용하고 실제 읽은 바이트 수를 `EAX` 전체에 반환하자 `Not PTX file` 및 `exit(-1)` 경로가 사라졌습니다. 40초 관찰 동안 원본 실행은 종료하지 않고 약 420만 dispatch와 지속적인 heartbeat/progress를 보였습니다. 이는 PIU.DAT 전용 우회가 아니라 DOS/4GW 보호 모드 file-read ABI 복원입니다.

**Confirmed:** At read #26, the `INT 21h AH=3Fh` entry EIP was `0x030F87B7`, with return address `0x030F53F3`. The stack simultaneously retained request candidate `0x00854D00`, full payload size `0x00855C29`, and destination `0x0393F050`, disproving an earlier 16-bit truncation in the RES parser or upper Watcom caller.

The original wrapper executes `mov ecx, ebx; mov ah, 3fh; int 21h` and consumes a 32-bit `EAX` result. Its caller subtracts that result from a 32-bit remaining count. Only the HLE reduced the request to `ECX & 0xffff` and returned a 16-bit `AX`. Restoring full `ECX/EAX` removes the `Not PTX file`/`exit(-1)` path. A 40-second observation remained live for roughly 4.2 million dispatches with continuing heartbeat and progress, without adding archive-specific behavior.

## RES 아카이브 포맷 확정과 자산 계층 (2026-07-22 Task 260) / Confirmed RES Archive Format and Asset Layering

**확인됨 (포맷).** `SPR.RES`와 `DATAS/PIU.DAT`은 **동일한 `RES\0` 포맷**이다.

```c
struct ResHeader {          // 0x10 bytes
    char     magic[4];      // "RES\0"
    uint32_t version;       // 1
    uint32_t entry_count;
    uint32_t data_bytes;    // 데이터부 총 크기
};
struct ResEntry {           // 24 bytes
    char     name[16];      // NUL 패딩
    uint32_t size;
    uint32_t offset;        // 데이터 base 기준 상대 오프셋
};
// 데이터 base = 0x10 + entry_count * 24
```

| 아카이브 | 엔트리 | data base | 내용 |
|---|---:|---|---|
| `SPR.RES` (105,473 B) | 344 | `0x2050` | 전부 `.SPR` |
| `DATAS/PIU.DAT` (8,751,057 B) | 465 | `0x2BA8` | 전부 `.PTX` |

이는 위 절의 `data_bytes = 0x00855C29` 관측과 정합한다(`PIU.DAT` 헤더 오프셋 0x0C).

**확인됨 (무결성).** 두 아카이브 모두 엔트리 범위 초과 0건, 간극 0건, 겹침 0건이며,
정렬한 마지막 엔트리의 `offset+size`가 파일 크기와 정확히 일치하고 헤더의
`data_bytes`도 실측과 일치한다. **아카이브 자체는 손상이 없다.**

**확인됨 (`.SPR`은 텍스트 스크립트).** `.SPR`은 픽셀을 담지 않는다.

```
1.SPR      "TYPE ANI\r\nNUM 3\r\n..."
108.SPR    "TYPE TILE\r\nNUM 1..."
```

`TYPE ANI` / `TYPE TILE` + `NUM n` 형태로 **어떤 PTX 텍스처의 어느 영역을 어떻게
배치·애니메이션할지** 기술한다. 크기도 54~4,568 B에 불과하다. 실제 픽셀은 전부
`PIU.DAT`의 `.PTX`에 있다. 게임이 이를 `strtok` 계열로 토큰 파싱하는 것과 정합한다.

```mermaid
flowchart LR
    SPRRES["SPR.RES<br/>344 × .SPR<br/>텍스트 스크립트"] -->|참조| PIUDAT["PIU.DAT<br/>465 × .PTX<br/>실제 픽셀"]
```

**확인됨 (확장자 치환).** 게임은 `.TGA`를 요청하지만 자산은 `.PTX`다. 위 절의
`hfont1.tga` → `HFONT1.PTX` 관측이 일반 규칙임을 확인했다: `logo_a.tga`/`logo_m.tga`
열기가 실패하는 한편 `PIU.DAT`에 `LOGO_A.PTX`(@0x1228), `LOGO_M.PTX`(@0x1240)가 있다.
`SPR.RES`에는 `.PTX`가 하나도 없다.

```
*.SPR  →  SPR.RES 조회
*.TGA  →  .PTX 로 치환  →  PIU.DAT 조회
```

**확인됨 (개별 파일 열기 실패는 정상).** 실행 추적에서 열기 **109건 중 77건이
err=2**로 실패한다(`white.spr`, `logo_a.tga`, `clearbk.spr`, `st_*.spr` 등). 결함이
아니라 **개별 파일을 먼저 시도하고 없으면 아카이브에서 읽는 오버라이드 관례**다.
성공한 32건은 CD에 실물로 존재하는 것뿐이다. **이 실패 로그를 "자산 로딩 실패"로
읽으면 오진이다.**

**확인됨 (read ABI 수정이 유지됨).** 위 절에서 복원한 32-bit read ABI가 현재도
유효하다. `PIU.DAT` 실측: `off=14848 want=8735744 got=8735744`(나머지 전체를 한 번에),
이어 `off=8750592 got=465`(EOF), 최종 seek이 `8751057` = 파일 크기와 일치. read·seek
오류 0건. `SPR.RES`도 105,473 B 전량 읽힌다.

**주의 (거짓 양성).** 요청보다 적게 반환되는 read는 대부분 **정상 EOF**다(게임이
4096씩 읽다 마지막에 잔여만 받음). "short read = 결함"으로 계측하면 오탐이 쏟아진다.

**미확정 (`.PTX` 픽셀 포맷).** `LOGO_A.PTX`(9,944 B) 헤더는
`50 54 58 00 | 00 01 20 00 20 00 82 00 2C 01 00 00 | FA FE 8A 28 A2 8A 28 A2 …`.
매직 뒤 12바이트를 u16 6개로 읽으면 거의 모든 파일이 동일한
`(256, 32, 32, 129, 300, 0)`이고 일부만 `130`이다. 파일 크기는 7~26 KB로 제각각인데
이 필드는 고정이므로 **해상도 필드가 아니다.** 데이터부의 `8A 28 A2` 반복 패턴으로
보아 압축 또는 팔레트 기반일 가능성이 있다.

**관련 미해결.** 실행 중 Glide로 올라오는 텍스처는 256×256 두세 개뿐인데 PTX는
465개다. PTX 디코드→텍스처 업로드 경로가 어디서 멈추는지가 배경 미표시
(`docs/analysis/glide2x-ovl-and-opengl-hle.md` Task 259)의 다음 관문이다.

**관측 방법.** `REPIU_DOS_ASSET_TRACE=1`로 구동하면 열기(성공·실패 전건), 읽기
(파일명·오프셋·요청/실제 바이트), 시크가 stderr에 남는다. 핸들이 아니라 **DOS
경로명**으로 기록된다.

**Confirmed (Task 260).** `SPR.RES` and `PIU.DAT` share one `RES\0` format: a
0x10-byte header (magic, version, entry count, data size) followed by 24-byte
entries of `char name[16]; uint32 size; uint32 offset`, data based at
`0x10 + count * 24`. Both verify exactly — no out-of-range entries, no gaps, no
overlaps, last entry ending on the file size, header `data_bytes` matching the
measured payload. `.SPR` entries are **text scripts** (`TYPE ANI`/`TYPE TILE`/
`NUM n`) describing placement and animation, consistent with `strtok`-style
parsing; the pixels live in `.PTX`. The `hfont1.tga` → `HFONT1.PTX` substitution
noted above is the general rule. Two things that read like defects are normal: 77
of 109 loose-file opens fail with error 2 because the game tries a loose override
before the archive, and most short reads are ordinary EOF from a 4096-byte loop.
The restored 32-bit read ABI still holds — `PIU.DAT` is read to its last byte with
zero errors. Open: the `.PTX` pixel format, whose post-magic fields are nearly
constant across files of very different sizes and therefore are not dimensions.
