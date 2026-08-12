# Glide2x.ovl과 OpenGL HLE 분석

## 확인된 바이너리 구조

**확인됨.** 사용자 asset `PIU/Glide2x.ovl`은 MZ 뒤에 LE image를 가진 80386 OS/2 module입니다. 현재 LE parser로 3개 object, 78개 page, 3,419개 internal fixup을 해석할 수 있습니다. import module은 없습니다. 그러나 이 코드를 실행하면 원본 3Dfx 하드웨어·PCI·port I/O 경로까지 함께 요구하므로 host OpenGL 연동에는 module 전체 실행보다 export facade가 적합합니다.

resident-name table에는 module 이름 `glide2x`와 ordinal 1~172가 들어 있습니다. 주요 예는 다음과 같습니다.

| ordinal | decorated export | 확인 가능한 ABI |
|---:|---|---|
| 32 | `_GRGLIDEINIT@0` | 인자 0바이트 |
| 38 | `_GRSSTSELECT@4` | 인자 4바이트 |
| 71 | `_GRDRAWPOINT@4` | 인자 4바이트 |
| 72 | `_GRDRAWLINE@8` | 인자 8바이트 |
| 73 | `_GRDRAWTRIANGLE@12` | 인자 12바이트 |
| 74 | `_GRDRAWPLANARPOLYGON@12` | 인자 12바이트 |
| 75 | `_GRDRAWPLANARPOLYGONVERTEXLIST@8` | 인자 8바이트 |
| 76 | `_GRDRAWPOLYGON@12` | 인자 12바이트 |
| 77 | `_GRDRAWPOLYGONVERTEXLIST@8` | 인자 8바이트 |
| 85 | `_GRBUFFERSWAP@4` | 인자 4바이트 |
| 89 | `_GRCLIPWINDOW@16` | 인자 16바이트 |
| 94 | `_GRCULLMODE@4` | 인자 4바이트 |
| 112 | `_GRLFBLOCK@24` | 인자 24바이트 |
| 118 | `_GRSSTWINOPEN@28` | 인자 28바이트 |
| 138 | `_GRTEXSOURCE@16` | 인자 16바이트 |
| 144 | `_GRTEXDOWNLOADMIPMAPLEVELPARTIAL@40` | 인자 40바이트 |

**확인됨 (2026-07-14).** 그리기 API 71~77, `_GRCLIPWINDOW@16`(89), `_GRCULLMODE@4`(94)는 resident-name table 재파싱으로 ordinal과 인자 byte 수를 재확정했습니다. 이 중 71~76은 HLE stub이 등록되어 있고 77 `_GRDRAWPOLYGONVERTEXLIST@8`은 아직 미등록입니다.

`@N` suffix는 callee가 소비하는 인자 byte 수를 제공하므로, `GETPROCADDR`가 요청된 이름을 resident-name table과 대응시키면 executable별 고정 주소 없이 guest-callable trap ABI를 만들 수 있습니다. 실제 PIU가 요청하는 export 집합은 아직 동적 trace로 확정해야 합니다.

## 권장 연동 구조

```mermaid
flowchart LR
    PIU["원본 PIU x86"] --> LIN["LINEXE HLE<br/>LOADMODULE / GETPROCADDR"]
    LIN --> GATE["동적 Glide trap gate<br/>name + ordinal + @N"]
    GATE --> ABI["guest ABI decoder"]
    ABI --> STATE["플랫폼 공용 Glide 2 state"]
    STATE --> CMD["render commands/resources"]
    CMD --> GL["Win32 OpenGL backend"]
    GL --> GPU["host GPU/window"]
```

`LINEXE_LOADMODULE("glide2x.ovl")`은 OVL 코드를 실행하지 않고 검증된 virtual module handle을 반환합니다. `LINEXE_GETPROCADDR`는 요청 이름 또는 ordinal을 export metadata와 대조하고, 처음 요청된 함수에 안정적인 합성 far pointer를 할당합니다. pointer의 trap은 서비스 id와 stack-byte count를 포함합니다. 실제 호출 시 dispatcher가 guest stack에서 인자를 복사하고 반환값과 callee stack cleanup을 원본 ABI에 맞게 적용합니다.

Glide 상태는 플랫폼 공용 계층에서 관리합니다. OpenGL backend는 다음을 담당합니다.

* `grSstWinOpen/Close`, `grBufferSwap`: host window, context, front/back buffer
* `grDrawTriangle`와 vertex list: `GrVertex`를 읽어 vertex buffer와 draw command 생성
* color/alpha/depth/fog/chroma/texture combine: Glide state key를 GLSL shader variant 또는 uniform으로 변환
* texture download/source: 3Dfx texture-memory address를 virtual allocation으로 관리하고 OpenGL texture에 upload
* `grLfbLock/Unlock`: guest arena의 staging buffer를 제공하고 lock 방향에 따라 framebuffer와 동기화
* query/status: 일관된 virtual Voodoo capability와 deterministic status 반환

OpenGL compatibility fixed-function 호출에 직접 매핑하면 초기 prototype은 짧지만 Glide의 color/alpha/texture combine을 정확히 표현하기 어렵고 OpenGL core/WebGL 확장을 막습니다. 플랫폼 공용 state translator와 shader 기반 backend가 장기 구조로 적합합니다. Khronos도 구식 fixed-function 기능이 core profile에서 제거되고 현대 구현에서는 shader로 대체된다고 설명합니다.

## 구현 순서와 검증 경계

1. `LOADMODULE` virtual handle과 `GETPROCADDR` 요청 trace만 구현합니다.
2. trace에서 실제 PIU export 집합, 호출 순서, stack/return ABI를 확정합니다.
3. init/query/window/clear/swap의 최소 backend로 첫 화면을 엽니다.
4. triangle, render state, texture 순으로 구현하여 frame hash와 screenshot을 비교합니다.
5. LFB, fog, gamma, movie helper처럼 실제 호출된 후순위 기능을 추가합니다.

OVL의 GPL 계열 공개 source나 기존 Glide wrapper 구현은 코드로 복사·번역·링크하지 않습니다. 명세, export metadata, PIU runtime trace를 이용한 clean-room 구현만 허용합니다.

## 외부 근거

* [3Dfx Glide 2.4 Programming Guide](https://www.bitsavers.org/components/3dfx/Glide_Programming_Guide_2.4_199707.pdf)
* [3Dfx Glide 2.0 API Reference](https://www.gamers.org/dEngine/xf3D/glide/glideref.htm)
* [Khronos OpenGL Registry](https://registry.khronos.org/OpenGL/index_gl.php)
* [Khronos OpenGL fixed-function pipeline 설명](https://wikis.khronos.org/opengl/Fixed_Function_Pipeline)
* [MAME machine record: Pump It Up 1st / Voodoo Banshee](https://arcade.vastheman.com/minimaws/machine/pumpit1)

## 600초 프레임 루프 관측과 검은 화면 근인 (2026-07-19 Task 249) / 600-Second Frame-Loop Observation and Black-Screen Root Cause

**확인됨.** aot-dynamic 600초 구동에서 Glide 초기화 시퀀스(게이트 전수 로그 96건:
init→queryHardware→select→winOpen→화면 크기→TMU 주소→상태 일괄 설정→state
round-trip→텍스처 mem/다운로드/샘플러→hints→프레임 루프 진입)가 거부 0건으로
통과했고, 78초부터 종료까지 프레임 루프
`grBufferClear→grColorMask→grBufferSwap→grBufferNumPending`(+ 프레임별 상태
재설정: cullMode/depthMask/fogMode/clipWindow/texMipMapMode 등)이 안정 순환했다.
창은 실제 WGL 창이다(`mode_supported=1 opened=1`).

**확인됨 (검은 화면 = 렌더 경로 3계층 no-op).** (1) `_GRBUFFERSWAP@4` 핸들러가
`SwapBuffers`를 호출하지 않으므로 `OpenWindowed`의 최초 1회 검정 clear+swap 이후
어떤 프레임도 제시되지 않는다. (2) `_GRBUFFERCLEAR@12`와 draw 계열(71~76)이
no-op이므로 백버퍼 내용이 없다. (3) 텍스처 다운로드/소스가 텍셀 데이터를 버린다.
단계별 보완 계획은 `docs/design/20260719-249-glide-render-path-completion.md`.

**미확정.** 600초 동안 draw(66~77)/LFB(110~117) ordinal이 1 Hz 샘플에 전혀 나타나지
않았다 — 게임이 Glide 밖 하위 시스템(입력 I/O, YMZ280B, CD 오디오, EEPROM) 대기로
콘텐츠 단계에 도달하지 못했을 가능성과 관측 캡(게이트 로그 96건, 1 Hz 샘플) 가능성이
남아 있으며, ordinal별 라이브 카운트 텔레메트리(계획 R0)로 확정한다.

**Confirmed (Task 249).** A 600 s aot-dynamic run passes the full Glide startup
sequence with zero rejected gates and settles from 78 s into a stable frame loop
(clear→colorMask→swap→numPending plus per-frame state resets) on a real WGL
window. The black screen is structural: no present (`grBufferSwap` never calls
`SwapBuffers` after the single black clear at open), no drawing (clear/draw
family are no-ops), no textures (downloads discarded). **Open:** no draw/LFB
ordinal appeared in ~560 samples — either the game is content-blocked on
non-Glide subsystems or the observation caps missed rare calls; the R0
per-ordinal live counts in design 249 settle this.

## PIU.EXE가 참조하는 전체 Glide export 집합 (2026-07-19 Task 249) / Full Glide Export Set Referenced by PIU.EXE

**확인됨.** `PIU.EXE` 바이너리 문자열 스캔으로 장식된 Glide 이름 **97개**를 확인했다
(`_GR[A-Z0-9]+@N` 패턴, GETPROCADDR 요청 후보 전체 집합). `glide2x.ovl`
resident-name table 전수 파싱으로 ordinal 0(모듈명 `glide2x`)~172를 확정했으며,
1~7·64·78·108·109·133·141·142·145는 내부 헬퍼(`__GRTEXDOWNLOAD_DEFAULT_*`,
`__GRTEXDOWNLOADPALETTE@16` 등), 9~28·51~55·57~63·65·146은 `_GU*` 유틸리티,
147~172는 PCI/포트 I/O 계열(`_PIO*`, `_PCI*`)로 HLE 대상이 아니다. 현재 HLE
시그니처 카탈로그는 44개이므로 PIU 참조 이름 기준 **53개가 미등록**이다. 미등록
기능군: LFB 계열 7개(grLfbLock@24/Unlock@8/WriteRegion@32/ReadRegion@28 등),
크로마키 2개, 상수색 2개, AA draw 5개, grDrawPolygonVertexList@8, 텍스처
다운로드/테이블 5개, 텍스처 파라미터 6개, SST 상태/동기화 12개, fog/감마/기타 6개,
유틸/진단 8개. 전체 목록과 ordinal은
`docs/design/20260719-249-glide-render-path-completion.md` §4 참조.

**Confirmed (2026-07-19 Task 249).** A string scan of `PIU.EXE` yields **97**
decorated Glide names (the full GETPROCADDR candidate set). A full parse of the
OVL resident-name table pins ordinals 0-172; internal helpers, `_GU*` utilities,
and the PCI/port-I/O family (147-172) are out of HLE scope. The current
44-signature catalog leaves **53 referenced names unregistered** (LFB, chroma
key, constant color, AA draws, texture download/table/parameter, SST
status/sync, fog/gamma, and diagnostics groups). See design 249 §4 for the full
table.

# Glide2x.ovl and OpenGL HLE Analysis

The user-provided `Glide2x.ovl` is an MZ-bound 80386 LE module with three objects, 78 pages, 3,419 internal fixups, and no imported modules. Its resident-name table exposes 172 decorated exports. Suffixes such as `_GRSSTWINOPEN@28`, `_GRDRAWTRIANGLE@12`, and `_GRBUFFERSWAP@4` provide the callee argument-byte count needed for dynamic guest trap gates.

**Confirmed (2026-07-14).** A resident-name table re-parse re-established the ordinals and argument-byte counts of the drawing group 71–77 (`_GRDRAWPOINT@4`, `_GRDRAWLINE@8`, `_GRDRAWTRIANGLE@12`, `_GRDRAWPLANARPOLYGON@12`, `_GRDRAWPLANARPOLYGONVERTEXLIST@8`, `_GRDRAWPOLYGON@12`, `_GRDRAWPOLYGONVERTEXLIST@8`), plus `_GRCLIPWINDOW@16` (89) and `_GRCULLMODE@4` (94). HLE stubs cover 71–76; ordinal 77 `_GRDRAWPOLYGONVERTEXLIST@8` is not registered yet.

The recommended design treats the OVL as export metadata rather than executing its 3Dfx PCI and port-I/O implementation. LINEXE returns a virtual module handle, resolves requested names/ordinals to stable synthetic far pointers, and dispatches traps through a platform-neutral Glide 2 state machine. A Win32 OpenGL backend translates draw state, textures, buffers, LFB staging, and queries. A shader-based translator is preferred over direct legacy fixed-function calls because it can model Glide combine behavior and later support core OpenGL and WebGL backends.

Implementation should first trace the exact PIU export set and ABI, then add initialization/window/clear/swap, triangle rendering, state, textures, and finally LFB or other observed helpers. Existing GPL-family Glide implementations remain behavioral references only; implementation must be clean-room from published documentation, asset metadata, and runtime evidence.

## 동적 HLE frontier (2026-07-11)

**확인됨.** LE resident-name parser와 ordinal별 동적 gate를 연결한 뒤 PIU의 초기 호출 순서는 다음과 같습니다.

1. `_GRGLIDEINIT@0`
2. `_GRSSTQUERYHARDWARE@4` — `GrHwConfiguration*`; `num_sst=1`만으로 통과
3. `_GRSSTSELECT@4` — board index `0`
4. `_GRSSTWINOPEN@28`

`GETPROCADDR` 출력은 packed 16:16 pointer가 아니라 `{32-bit linear address, 32-bit client CS}`입니다. PIU는 이를 읽어 자신의 code stub을 `E9 rel32`로 self-patch한 뒤 gate로 jump합니다. gate의 stack은 `[return EIP, arguments...]`이고 decorated `@N`과 일치하는 callee cleanup이 필요합니다.

관찰된 `grSstWinOpen` 인자는 `hWnd=0`, resolution `7`, refresh `0`, color format `1`, origin `1`, color buffer `2`, auxiliary buffer `1`입니다. 다음 결정은 논리 640×480 surface를 windowed presentation으로 제공할지, 원본에 가까운 exclusive fullscreen으로 제공할지입니다.

```mermaid
flowchart LR
    INIT["grGlideInit"] --> QUERY["QueryHardware<br/>num_sst=1"]
    QUERY --> SELECT["SstSelect(0)"]
    SELECT --> OPEN["SstWinOpen<br/>0,7,0,1,1,2,1"]
    OPEN --> POLICY{"host presentation policy"}
    POLICY --> WINDOW["640x480 logical window"]
    POLICY --> FULL["exclusive fullscreen"]
```

The dynamic path now confirms the startup sequence `grGlideInit`, `grSstQueryHardware`, `grSstSelect(0)`, and `grSstWinOpen`. `GETPROCADDR` returns an eight-byte `{linear address, client CS}` pair; PIU self-patches a near-jump stub to each synthetic gate. The observed open arguments are `0,7,0,1,1,2,1`. The next decision is windowed presentation of a 640×480 logical surface versus exclusive fullscreen.

## Windowed OpenGL 결과와 signature frontier

**확인됨.** 1번 정책으로 640×480 client window, double-buffered WGL context와 24-bit depth-capable pixel format을 생성했습니다. `grSstWinOpen`은 FXTRUE를 반환했고 다음 export는 ordinal 39 `_GRSSTSCREENWIDTH@0`입니다.

이 함수는 인자가 없지만 반환값은 x87 `ST(0)`의 floating-point 값입니다. resident export의 `@N` suffix는 stack cleanup 크기만 제공하며 반환 type, pointer target 구조, enum 의미를 제공하지 않습니다. 따라서 다음 구현 전에는 공식 Glide 2 문서에서 파생한 명시적 signature catalog를 유지할지, 개별 handler에 ABI 지식을 분산할지 결정해야 합니다.

```mermaid
flowchart LR
    OPEN["WGL window open 성공"] --> WIDTH["grSstScreenWidth@0"]
    WIDTH --> ABI{"return ABI"}
    ABI --> CAT["typed signature catalog"]
    ABI --> ADHOC["per-handler ad-hoc ABI"]
```

The windowed policy successfully creates a 640×480 client window and double-buffered WGL context with a depth-capable pixel format. The next export is ordinal 39 `_GRSSTSCREENWIDTH@0`, which returns a floating-point value in x87 `ST(0)`. Decorated `@N` metadata provides stack cleanup only, making a typed Glide 2 signature catalog versus scattered per-handler ABI knowledge the next architectural decision.

## Typed catalog 이후 texture-memory frontier

**확인됨.** 점진적 typed catalog와 별도 x87 context helper를 추가한 뒤 `grSstScreenWidth()`의 `640.0f`, `grSstScreenHeight()`의 `480.0f`가 정상 소비됐습니다. 이어서 `grTexMinAddress(GR_TMU0)`가 호출됐고 0을 반환한 뒤 다음 frontier는 ordinal 45 `_GRTEXMAXADDRESS@4`, 인자 TMU 0입니다.

`grTexMaxAddress`는 memory byte count가 아니라 가장 작은 1×1 texture를 시작할 수 있는 마지막 8-byte aligned address입니다. PIU 1st hardware 자료는 Voodoo Banshee를 지목하지만, virtual TMU address space를 standard 2 MiB 호환값으로 제한할지 4 MiB로 노출할지는 host OpenGL resource 정책과 guest texture allocator를 함께 결정합니다.

The incremental typed catalog and separate x87 context helper successfully return `640.0f` and `480.0f`. After `grTexMinAddress(GR_TMU0)` returns zero, the next frontier is ordinal 45 `_GRTEXMAXADDRESS@4`. This value is the last 8-byte-aligned start address for the smallest texture, not a byte count. The next decision is a conservative 2 MiB or expanded 4 MiB virtual TMU address space.

## 8 MiB TMU 이후 초기 render state

**확인됨.** 선택된 8 MiB TMU는 `grTexMaxAddress(GR_TMU0)=0x007FFFF8`로 반환됐습니다. 이어지는 초기 상태 호출은 다음과 같습니다.

| API | 인자 | 처리 |
|---|---|---|
| `grColorMask` | `1,0` | RGB write on, alpha off |
| `grRenderBuffer` | `1` | OpenGL back buffer |
| `grDepthMask` | `1` | depth write on |
| `grDepthBufferMode` | `1` | Z-buffer mode |
| `grLfbWriteColorFormat` | `1` | 논리 LFB state에 보존 |

다음 frontier는 `_GRALPHACOMBINE@20`이며 인자는 `1,0,0,2,0`입니다. 이 시점부터 Glide combine equation을 legacy OpenGL fixed-function state로 근사할지 shader variant/uniform으로 정확히 표현할지 결정해야 합니다.

The selected 8 MiB TMU returns `0x007FFFF8`. PIU then enables RGB-only writes, selects the back buffer, enables depth writes and Z-buffering, and stores LFB color format 1. The next frontier is `_GRALPHACOMBINE@20` with arguments `1,0,0,2,0`, requiring a shader-state versus legacy fixed-function mapping decision.

## GLSL combine과 초기 raster state / GLSL combine and initial raster state

GLSL 1.10 program을 WGL context 생명주기 안에서 한 번 compile/link하고, 관찰된 alpha/color combine `1,0,0,2,0`을 uniform으로 전달했습니다. 두 식은 `LOCAL` iterated RGBA를 선택합니다. 이어서 `grAlphaBlendFunction(4,0,4,0)`은 `ONE,ZERO,ONE,ZERO`, alpha test와 depth compare의 값 `7`은 `ALWAYS`, fog mode `0`은 disabled로 확인했습니다. Blend 의미와 비활성 기준 호출은 [3dfx Glide Reference Manual 2.4](https://www.bitsavers.org/components/3dfx/Glide_Reference_Manual_2.4_199707.pdf)의 `grAlphaBlendFunction` 정의를 따릅니다.

```mermaid
flowchart LR
    AC["Alpha combine<br/>1,0,0,2,0"] --> GLSL["GLSL uniform program"]
    CC["Color combine<br/>1,0,0,2,0"] --> GLSL
    BL["Blend 4,0,4,0"] --> NOBLEND["GL_BLEND off"]
    AT["Alpha ALWAYS"] --> NOATEST["GL_ALPHA_TEST off"]
    DT["Depth ALWAYS"] --> DCOMP["glDepthFunc ALWAYS"]
    FOG["Fog 0"] --> NOFOG["GL_FOG off"]
```

live telemetry에 마지막 ordinal과 다섯 stack argument를 추가한 결과 다음 frontier는 ordinal 89 `_GRCLIPWINDOW@16`으로 확인됐습니다. 관찰 인자는 `(0,0,0x030FED90,0x030FED8B)`이며 뒤의 두 값은 유효한 640×480 좌표가 아니라 guest code 범위입니다. 호출 시점의 값 생성 경로 또는 import-stub/stack ABI를 역추적하기 전에는 scissor로 적용할 수 없습니다.

The WGL-owned GLSL 1.10 program is compiled and linked once, then receives the observed alpha/color combine `1,0,0,2,0` through uniforms. Both select local iterated RGBA. The following state resolves to `ONE,ZERO,ONE,ZERO` blending, `ALWAYS` alpha/depth comparisons, and disabled fog. Shared live telemetry identifies the next frontier as ordinal 89 `_GRCLIPWINDOW@16`; its observed `(0,0,0x030FED90,0x030FED8B)` contains guest-code addresses rather than valid 640×480 bounds, so no scissor mapping is applied pending producer/ABI tracing.

## Clip 좌표 손상과 반환 ABI 정정 / Clip Corruption and Return ABI Correction

위의 x87 반환 결론은 이후 원본 caller 역추적으로 반증됐습니다. return EIP `0x0304F7AF` 직전 코드는 `[EBX+0x1A90]`, `[EBX+0x1A94]`를 push하며, 초기화 루틴은 `grSstScreenWidth/Height` 직후 EAX를 이 필드에 저장합니다.

```mermaid
flowchart LR
    WIDTH["grSstScreenWidth"] --> EAXW["EAX = 640"]
    EAXW --> FW["[EBX+1A90]"]
    HEIGHT["grSstScreenHeight"] --> EAXH["EAX = 480"]
    EAXH --> FH["[EBX+1A94]"]
    FW --> CLIP["grClipWindow(0,0,640,480)"]
    FH --> CLIP
```

기존 HLE는 x87만 갱신하여 EAX에 import stub 주소를 남겼습니다. typed return을 `UInt32/EAX`로 정정하자 좌표가 `(0,0,0x280,0x1E0)`으로 복원됐습니다. 전체 clip을 OpenGL viewport/scissor로 적용하고 cull mode 0을 통과한 뒤 다음 frontier는 `_GRGLIDEGETSTATE@4`, 출력 포인터 `0x0383E180`입니다.

The earlier x87 conclusion was disproved by tracing the original caller. It stores EAX directly into integer width/height fields and later passes them to `grClipWindow`. Correcting the typed return to integer `UInt32/EAX` restored `(0,0,640,480)`. Full-window viewport/scissor and disabled culling then advance execution to `_GRGLIDEGETSTATE@4` with output pointer `0x0383E180`.

## Opaque Glide2 state round-trip

[Glide 2.4 reference manual](https://www.bitsavers.org/components/3dfx/Glide_Reference_Manual_2.4_199707.pdf)은 `GrState`를 공개 필드가 없는 Get/Set 쌍으로 정의합니다. [Glide 3.0 programming guide](https://www.bitsavers.org/components/3dfx/Glide_Programming_Guide_3.0_199806.pdf)는 이후 API에서 `GR_GLIDE_STATE_SIZE` 질의를 사용하도록 설명합니다. [공개 호환 구현의 312바이트 Glide2 관찰](https://www.zeus-software.com/forum/viewtopic.php?start=10&t=2232)은 PIU allocation 간격 336바이트와 교차 확인됐습니다. [DOS/32A 공식 저장소](https://github.com/amindlost/dos32a)에서는 Glide state 관련 근거를 찾지 못했으며 DOS runtime 호환성만으로 그래픽 API private layout을 추론하지 않습니다.

```mermaid
sequenceDiagram
    participant PIU
    participant HLE
    participant B as 312-byte buffer
    PIU->>HLE: grGlideGetState(0x0383E180)
    HLE->>B: deterministic logical snapshot
    PIU->>HLE: grGlideSetState(0x0383E180)
    HLE->>B: validate magic/version
    HLE-->>PIU: restore logical snapshot
```

동일 포인터 Get/Set 사이에 다른 Glide gate나 직접 blob 접근은 관찰되지 않았습니다. round-trip 통과 후 다음 frontier는 `_GRDITHERMODE@4(2)`입니다.

The public 312-byte Glide2 candidate is corroborated by PIU's 336-byte allocation gap. PIU performs an immediate same-pointer Get/Set round-trip without observed direct access, allowing an independent deterministic state image rather than reproducing a private vendor layout. DOS/32A contains no relevant Glide-state evidence. The next frontier is `_GRDITHERMODE@4(2)`.

## Host dithering 1단계 / Host Dithering Stage 1

**확인됨:** `_GRDITHERMODE@4(2)`를 `GL_DITHER` 활성화로 처리하고 mode를 logical state와 state-image version 2에 보존했습니다. 호출 이후 미구현 Glide gate가 아니라 원본 guest `EIP=0x030F968B` 접근 위반까지 진행했습니다. 따라서 host dither 연결은 현재 startup 경로를 통과합니다.

**TODO:** 현대 32-bit OpenGL framebuffer의 `GL_DITHER`는 Voodoo의 16-bit ordered dithering과 픽셀 동일성을 보장하지 않습니다. mode-2 matrix, PIU color quantization, reference capture를 확보한 뒤 GLSL ordered dithering으로 교체하거나 선택 가능하게 만들어야 합니다.

```mermaid
flowchart LR
    MODE["grDitherMode(2)"] --> GL["GL_DITHER enabled"]
    GL --> PASS["Glide startup passes"]
    PASS --> AV["guest AV at 0x030F968B"]
    GL -. fidelity TODO .-> GLSL["verified ordered-dither shader"]
```

**Confirmed:** `_GRDITHERMODE@4(2)` enables `GL_DITHER` and persists mode 2 in logical state and state-image version 2. Execution passes the Glide startup path and later reaches a guest access violation at `0x030F968B`, not an unimplemented Glide gate. Exact Voodoo ordered dithering remains a GLSL fidelity TODO requiring matrix, quantization, and reference-capture evidence.

## `grTexMaxAddress` 및 `grTexMinAddress` 인자 ABI 보정 / `grTexMaxAddress` and `grTexMinAddress` ABI Correction

**정정됨 (2026-07-18 Task 235, 이 절의 기존 cdecl 결론은 반증됨):** Task 234가 기록했던 "게임 바이너리가 cdecl로 컴파일되어 `ESP += 4`(인자 미pop)로 처리해야 한다"는 결론은 **오류였다.** xref 추적으로 두 thunk의 유일한 호출자가 `fxTMInit`임을 확인했고, `fxTMInit`은 `push arg; call grTexMin; push arg; call grTexMax; mov eax,[esp]`처럼 caller측 정리 없이 호출한다 — 즉 **stdcall(피호출자 인자 pop, `ESP += 8`)을 전제**한다. cdecl 처리(`ESP += 4`)는 잔여 인자 2개가 `mov eax,[esp]`를 오염시켜 fxTMInit NULL gc 크래시(0x0304DBF8)를 일으키는 회귀였다. 현재 HLE는 stdcall(`Esp += 2*4`)로 복원되어 있다. 상세: `docs/analysis/current-execution-frontier.md`의 Task 235 절.

**Corrected (2026-07-18 Task 235; the earlier cdecl conclusion in this section is disproved):** Task 234's claim that the game binary calls these as `cdecl` (requiring `ESP += 4`) was wrong. xref tracing shows `fxTMInit` is the sole caller of both thunks and issues `push arg; call ...` with no caller-side cleanup, i.e. it assumes **stdcall (callee pops, `ESP += 8`)**. The cdecl handling left two stale dwords that corrupted `mov eax,[esp]` and caused the fxTMInit NULL-gc crash at `0x0304DBF8`. The HLE has been restored to stdcall (`Esp += 2*4`). Details: the Task 235 entry in `docs/analysis/current-execution-frontier.md`.

## GrVertex first-call observation (2026-07-20)

**Confirmed.** Direct `pumpit1` loader execution with `aot-dynamic` reached `_GRDRAWTRIANGLE@12`. Its three readable guest pointers were `0x0383C640`, `0x0383C67C`, and `0x0383C6F4`. The first pointer difference is `0x3C` (60 bytes), not the previously assumed 72 bytes. The first dwords decode as plausible screen coordinates: `(288.0, 329.9375)`, `(296.0, 329.9375)`, `(288.0, 313.9375)`; dword 3 is `255.0` and dword 9 is `1.0` for all three.

**Inferred.** The triangle references entries 0, 1, and 3 of a 60-byte producer layout. The 72-byte Glide 2.4 layout must not be applied to this PIU call path without reconciling that stride; the 72-byte capture includes adjacent entry data.

## GrVertex 60바이트 레이아웃 확정 및 투영 근인 (2026-07-20/21 Task 254) / Confirmed 60-byte GrVertex Layout and Projection Root Cause

**확인됨 (정점 레이아웃).** 첫 삼각형 3정점의 15 dword(60바이트) stride를 실측
디코드한 결과, 표준 Glide GrVertex(2 TMU)와 정확히 일치한다. dword별 V0/V1/V2 실측값:

| dword | 필드 | V0 | V1 | V2 |
|---:|---|---|---|---|
| 0 | x (screen) | 288.0 | 296.0 | 288.0 |
| 1 | y (screen) | 329.9375 | 329.9375 | 313.9375 |
| 2 | z (Glide 무시) | 정크 | 정크 | 정크 |
| 3 | r [0..255] | 255.0 | 255.0 | 255.0 |
| 4 | g [0..255] | 0.0 | 0.0 | 0.0 |
| 5 | b [0..255] | 0.0 | 0.0 | 0.0 |
| 6 | ooz (65535/z) | 가변 | 가변 | 가변 |
| 7 | a [0..255] | 255.0 | 255.0 | 255.0 |
| 8 | oow (1/w) | 1.0 | 1.0 | 1.0 |
| 9 | tmu0.sow | 72.0 | 80.0 | 72.0 |
| 10 | tmu0.tow | 32.0 | 32.0 | 48.0 |
| 11..14 | tmu0.oow, tmu1.* | 가변 | 가변 | 가변 |

이 삼각형은 **불투명 빨강(r=255,g=0,b=0,a=255) + 텍스처 좌표**를 가진다. 즉 게임은
색과 텍스처를 갖춘 정상 지오메트리를 제출하고 있으며, 렌더 부재는 데이터 문제가
아니다.

**확인됨 (검은 화면 근인 = 투영 부재).** 정점 x/y는 640×480 화면 픽셀 좌표인데
`GlideOpenGlBackend`가 직교 투영(`glOrtho`)을 설정하지 않아, `ftransform()` 기반
정점 셰이더가 단위 투영행렬을 적용하면 픽셀 좌표가 NDC `[-1,1]` 밖으로 나가 모든
삼각형이 클리핑된다. 240초 구동에서 삼각형은 거부 0·미처리 0으로 안정 제출되지만
화면에 나타나지 않는 이유가 이것이다.

> **정정됨 (2026-07-22 Task 259).** 아래의 **"origin=1 = GR_ORIGIN_UPPER_LEFT"는
> 반증됐다.** 표준 `GrOriginLocation_t`는 `GR_ORIGIN_UPPER_LEFT`=0,
> `GR_ORIGIN_LOWER_LEFT`=1이므로 관측값 1은 **LOWER_LEFT**다. y 뒤집힌 투영을
> 고정으로 적용한 결과 화면 전체가 상하 반전되어 표시됐다(사용자 육안 확인).
> Task 254의 "투영 부재가 클리핑을 유발했다"는 근인 분석 자체는 유효하며, 잘못된
> 것은 방향뿐이다. 상세는 이 문서 하단 Task 259 절 참조.

**수정 (Task 254).** `OpenWindowed`에 ~~y 뒤집힌~~ 직교 투영을 추가한다
(~~`glOrtho(0, w, h, 0, -1, 1)`, 관측된 `grSstWinOpen` origin=1 =
GR_ORIGIN_UPPER_LEFT에 맞춤~~ → **Task 259에서 origin 인자를 실제로 존중하도록
정정**; `grCullMode(0)`으로 컬링이 꺼져 와인딩 반전 무해).
셰이더 Initialize에서 combine function uniform을 1(LOCAL)로 기본 설정해 흑색
프래그먼트를 예방한다. 색 반영(현재 흰색 고정)과 텍스처 샘플링은 후속 Task
255/256에서 다룬다. 상세: `docs/design/20260721-254-glide-screen-space-projection.md`.

**Confirmed (vertex layout).** Decoding the 15-dword (60-byte) stride of the first
triangle's three vertices matches the standard 2-TMU Glide GrVertex exactly (see
table). The triangle is opaque red (255,0,0,255) with texture coordinates, so the
game submits well-formed geometry — the absent rendering is not a data problem.

## R2 정점 색상 / R3 텍스처 combine 확정 (2026-07-21 Task 255) / R2 Vertex Color and R3 Texture Combine Confirmed

> **정정됨 (2026-07-22 Task 258).** 아래 절의 **"1×1 텍스처" 결론은 반증됐다.**
> 실제 텍스처는 **256×256**이며, 근거로 삼았던 "8바이트 간격"은 원본 게임의 성질이
> 아니라 **우리 `grTexTextureMemRequired` 버그가 만든 결과**였다. 상세는 이 문서
> 하단의 Task 258 절과 `docs/kb/glide-texture-lod-and-formats.md` 참조. 이 절의
> combine 전환·포맷·texel 값 관측은 유효하다.

**확인됨 (combine 전환).** 콘텐츠 draw는 `grColorCombine(function=3=SCALE_OTHER,
other=1=TEXTURE)`로 텍스처를 출력한다(init은 function=1=LOCAL=iterated 정점 색).
`grTexCombine(0,1,0,1,0,0,0)`도 관측. 텍스처는 startAddress 0(format 10=RGB565)과
8(format 12=ARGB4444), largeLod=0·aspect=3·evenOdd=3.
디코드 실측: addr=0 texel=(140,150,148,255) 불투명 회색, addr=8 texel=(0,0,0,0) 투명
검정. ~~largeLod=0·aspect=3 → **1×1**(8바이트 간격이 확증). 정점 텍스처 좌표는 1×1에서
wrap로 동일 texel을 샘플한다.~~ → **반증됨: 256×256이며 texel 값은 그 이미지의 첫
픽셀이다(Task 258).**

**구현 (Task 255).** 정점 색(r/g/b/a)을 `glColor4f`로 반영(R2), 플랫폼 공용 텍스처
디코드 모듈 + 백엔드 텍스처 캐시 + GLSL sampler2D로 SCALE_OTHER 텍스처를 샘플(R3).
투명 텍스처의 반투명 합성은 알파 블렌딩(후속)이 필요하다. 상세:
`docs/analysis/current-execution-frontier.md` Task 255,
`docs/design/20260721-255-glide-r2-r3-vertex-color-and-texture.md`.

**Confirmed (Task 255).** Content draws switch grColorCombine to function 3
(SCALE_OTHER, other = TEXTURE) to output the texture, versus init's function 1
(LOCAL, iterated vertex color). Textures at start addresses 0 (RGB565) and 8
(ARGB4444) are 1×1 (the 8-byte spacing confirms it); decode yields an opaque gray
texel at addr 0 and a transparent-black texel at addr 8. Implemented per-vertex
color (R2) and a platform-neutral decode module + backend texture cache + GLSL
sampler for SCALE_OTHER texture sampling (R3). Translucent compositing of
transparent textures needs alpha blending (follow-up).
**Confirmed (root cause = missing projection).** The x/y are 640×480 screen pixel
coordinates, but the backend sets no `glOrtho`, so the `ftransform()` vertex
shader with an identity projection pushes pixel coordinates outside NDC `[-1,1]`
and clips every triangle. **Fix (Task 254):** add a y-flipped
`glOrtho(0, w, h, 0, -1, 1)` in `OpenWindowed` (matching the observed
GR_ORIGIN_UPPER_LEFT; culling is disabled so reversed winding is harmless) and
seed the combine function uniforms to LOCAL. Color and texture sampling are
deferred to Tasks 255/256.
## PIU가 실제로 호출하는 Glide API 확정 (2026-07-21 Task 257) / Confirmed Set of Glide APIs PIU Actually Calls

**확인됨.** ordinal별 최초호출 감사(`REPIU_GLIDE_CALL_AUDIT`)로 `aot-dynamic
pumpit1` 300초 구동에서 카탈로그 97종 중 **39종 호출**, 거부 0건을 확정했다. 이전에는
게이트 진입 로그 96건 캡과 타임아웃 경로의 요약 누락 때문에 확정이 불가능했다.

런타임이 스스로 `unhandled (default)`로 기록한 것은 **정확히 3개**다:
`grConstantColorValue`(92), `grLfbLock`(112), `grLfbUnlock`(113). 나머지는 실동작
29종, 상태만 보존 2종, 전용 no-op 5종(`grHints`, `grTexClampMode/FilterMode/
MipMapMode`, `grTexCombine`)이다.

**반증됨 (드로우 계열 공백 가설).** "폴리곤·버텍스리스트 드로우 미구현이 BGA/UI
렌더 공백을 만든다"는 추정은 반증됐다. **드로우는 `grDrawTriangle`(73) 하나만
호출**되며 `grDrawPolygon`·`PlanarPolygon`·`PolygonVertexList`·`Point`·`Line`과
AA 계열 5종은 전부 미호출이다. 드로우 계열 확장은 실질 우선순위가 아니다.

**Confirmed (Task 257).** A per-ordinal first-call audit settles the reached set:
39 of 97 cataloged exports, zero rejected gates, and exactly three landing on the
default handler (`grConstantColorValue`, `grLfbLock`, `grLfbUnlock`). This also
disproves the earlier inference about missing polygon/vertex-list draws — only
`grDrawTriangle` is ever called.

## GrLOD_t 해석 오류와 첫 콘텐츠 렌더 (2026-07-22 Task 258) / GrLOD_t Misinterpretation and the First Real Content Render

**확인됨 (근인).** 게임이 정상 좌표의 콘텐츠 지오메트리를 제출해도 화면이 비던 근인은
`GrLOD_t`를 크기의 log2로 취급한 것이었다. `GrLOD_t`는 **열거값**이며 `GR_LOD_256`이
0, `GR_LOD_1`이 8이다(긴 변 = `256 >> lod`). 관측된 `largeLod=0·aspect=1x1` 다운로드는
**256×256**인데 **1×1**로 생성됐다 — 정확히 반전. 규약 상세는
`docs/kb/glide-texture-lod-and-formats.md`.

**결정적 증거.** 게임이 제출한 텍스처 좌표가 `st=(193,156)`, `(226,156)`, `(244,·)`로
1×1에는 존재할 수 없는 값이었다. 이 **불일치**가 근인으로 이끌었다.

**확인됨 (게스트 오염과 순환 논리).** `grTexTextureMemRequired`가 같은 계산을
공유하고 게임은 그 반환값으로 **자기 TMU 주소 공간을 배치**한다. 256×256 텍스처에
"8바이트"를 보고하자 게임이 텍스처를 8바이트 간격으로 쌓았고, Task 255는 그 간격을
1×1의 확증으로 기록했다. **우리 출력의 함수인 관측을 외부 사실로 취급**한 순환
논리였다. 수정 후 게임은 `0x2000` 간격으로 배치한다.

**수정과 검증.** LOD→변 길이 변환을 `8 - lod`로 정정하고, aspect ratio를 짧은 변에
적용하며, 유효성 검사를 `large_lod <= small_lod`로 반전했다. 콘텐츠 화면이 키 입력을
요구하므로(v0.0.77/78의 JAMMA 입력 수정 이후 정상 동작) `keybd_event` 합성 입력으로
구동했다: 텍스처 **256×256** 저장, 삼각형별 백버퍼 누적 `0 → 122 → … → 760`,
프레임 **1,765/307,200 비검정**(avg-rgb 133,133,133) swap #200까지 안정, 거부·미처리
0 유지. **이것이 rePIU의 첫 실제 콘텐츠 렌더다.**

**미확정.** (1) 정점 색 필드가 여전히 유효 범위 밖이다 — 현재 콘텐츠는 SCALE_OTHER라
정점색을 쓰지 않아 무해하나, 게임이 안 채우는 것인지 색 오프셋이 어긋난 것인지는
LOCAL combine 콘텐츠가 나와야 확정된다. (2) 텍스처 주소 간격 `0x2000`(8,192)은
256×256×2=131,072와 맞지 않아 mipmap/evenOdd 분할 또는 부분 다운로드 가능성이 있으며
`GrTexInfo` 실측이 필요하다. (3) LFB 경로는 실데이터 미검증이다.

**Confirmed (Task 258).** The black screen under valid geometry was a `GrLOD_t`
misinterpretation: it is an enumeration (`GR_LOD_256` = 0), not a log2 size, so
every texture was inverted — the observed `largeLod=0`/`1x1` download is 256x256
but was built 1x1. The decisive clue was a contradiction: textures were 1x1 while
the game submitted texture coordinates up to 244. Because
`grTexTextureMemRequired` shares the math and the guest lays out its own TMU
space from the answer, the error propagated into the game's 8-byte texture
spacing, which Task 255 had recorded as proof of 1x1 textures — a circular
argument built on our own output. After the fix the game spaces them 0x2000
apart. Verified with synthesized JAMMA input: 256x256 stores, per-triangle
readback climbing 0 → 760, and a frame stable at 1,765 non-black pixels — the
project's first real content render. Open: still-garbage vertex color fields, the
meaning of the 0x2000 spacing, and end-to-end LFB validation.

## 화면 상하 반전과 배경 미표시의 분리 (2026-07-22 Task 259) / Separating the Vertical Flip from the Missing Background

**확인됨 (반전 근인 = origin 인자 무시).** 화면 전체가 상하 반전되어 표시됐다.
근인은 두 겹이다. (1) `GrOriginLocation_t`는 `GR_ORIGIN_UPPER_LEFT`=0,
`GR_ORIGIN_LOWER_LEFT`=1인데 Task 254가 관측값 1을 UPPER_LEFT로 기록했다.
(2) 더 근본적으로 `origin` 인자가 백엔드로 **전달되지 않고** y 뒤집힌 투영이
하드코딩돼 있어, 게임이 어떤 값을 넘기든 같은 투영이 걸렸다. `OpenWindowed`가
origin을 받아 투영을 선택하도록 정정했다. LFB 블릿 쿼드는 lock origin과 창 투영
방향이 독립적으로 v를 뒤집으므로 XOR로 합쳤다(두 반전이 조용히 상쇄되는 것을 방지).

**확인됨 (배경 미표시 = Glide 밖 공백).** 참조 화면(파란 BGA 배경 + 로고)과 달리
배경 레이어 전체가 비었다. 전수 계측으로 렌더러 원인을 배제했다.

* **삼각형 센서스 4,000 draw 전수:** 단일 조합 `combine fn=3(SCALE_OTHER)
  other=1(TEXTURE) textured=1`, **최대 크기 232×39**. 640×480 배경도, 배경을
  구성할 타일도 존재하지 않는다.
* **배경 전달 가능 경로 전부 미호출:** `grTexDownloadMipMap@16`(47),
  `grTexDownloadMipMapLevelPartial@40`(144), `grLfbLock`(112)/`grLfbUnlock`(113).
* **포맷 센서스:** 이 화면이 쓰는 포맷은 `RGB_565`(10)와 `ARGB_4444`(12)뿐이며
  전부 지원·저장·디코드된다. 팔레트(`P_8`/`AP_88`)·NCC 포맷은 등장하지 않는다.

즉 **게임이 배경 draw를 발행하지 않는다.** 렌더러는 제출된 것을 정확히 그리며,
그 증거로 UI 텍스트가 참조 화면과 글자 모양·글로우·위치까지 일치한다.

**반증된 가설 2건 (이번 작업 중).** (1) "미지원 텍스처 포맷 때문" — 포맷 센서스로
반증. (2) "배경이 다른 combine 모드를 써서 텍스처가 꺼진다" — 삼각형 센서스로
반증(4,000 draw 전부 동일 조합·textured=1). 두 가설 모두 원인을 렌더러 쪽에서
찾으려는 편향이었고, 전수 집계가 이를 죽였다.

**미확정 (다음 과제).** 배경은 BGA(배경 동영상)로 보이며, 게임이 BGA 자산을
준비하지 못해 배경 렌더를 건너뛰는 것으로 **추정**되나 확인되지 않았다. 파일 I/O
경로 추적이 다음 작업이다. 이는 Glide 밖 영역이다.

**Confirmed (Task 259).** The upside-down screen had two layers: `GrOriginLocation_t`
has `GR_ORIGIN_UPPER_LEFT` = 0 and `GR_ORIGIN_LOWER_LEFT` = 1, so Task 254's
reading of the observed 1 as UPPER_LEFT was backwards — and more fundamentally
the argument never reached the backend, which hardcoded a y-flipped projection.
`OpenWindowed` now selects the projection from the origin it is given.

The missing background is *not* a renderer gap. A census over 4,000 draws shows a
single combine mode (SCALE_OTHER/TEXTURE, textured) and a maximum triangle of
232x39 — no background-sized geometry or tiles — while every alternative delivery
path (`grTexDownloadMipMap`, `...LevelPartial`, `grLfbLock`/`Unlock`) is never
called and every texture format in use is supported. The game simply never issues
background draws; the UI text matches the reference screen glyph for glyph, which
is what proves the renderer faithful. Two hypotheses were disproved along the way
(unsupported formats, and an unsupported combine mode disabling texturing), both
biased toward blaming the renderer. **Open:** whether the BGA asset path is what
blocks the background — a file-I/O investigation outside Glide.

## Task 302: depth comparison 3과 안전 gate decline / Depth comparison 3 and safe gate decline

**확인됨.** 사용자 장시간 로그는 약 880초에 `_GRDEPTHBUFFERFUNCTION@4(3)`을
처음 관찰했습니다. 기존 backend는 값 7만 받아 두 번 reject했고, 다음 depth-mask
frame 아래에 `0x0304F5A5, 3`이 남았습니다. guest는 이어서 `EAX=3`으로
`[eax+0x21F9]`, 즉 `0x000021FC`를 읽어 `0xC0000005`로 종료했습니다. 이는
Tasks 246-248의 미처리 gate stdcall frame 누수와 같은 구조입니다.

`SetDepthBufferFunction`은 이제 유효 Glide 비교 값 `0..7`을 OpenGL
`GL_NEVER..GL_ALWAYS`로 변환합니다. 또한 guest 반환 주소와 catalog signature가
검증된 이후의 specialized-handler 실패는 공용 safe-decline이 반환 kind에 따른
보수적 값과 정상 stdcall 정리를 수행합니다. hard reject는 반환 주소 불량과 signature
불일치만 남습니다.

Win32 x86 Debug 빌드가 성공했습니다. 30초 smoke run은 exit 0의 정상 timeout,
progress 542,996, Glide gate 49/49, `grDepthBufferFunction(7)` 3회 처리, reject/decline/
OpenGL error/caught exception 0을 기록했습니다. 인자 3의 실제 장시간 재현은 약 880초
frontier를 넘는 후속 interactive run으로 남습니다.

**Confirmed.** The user run first reaches `_GRDEPTHBUFFERFUNCTION@4(3)` at about
880 seconds. The former value-7-only backend rejected it twice, leaving
`0x0304F5A5, 3` beneath the next depth-mask frame. Guest code then read
`EAX(3) + 0x21F9 = 0x21FC` and terminated with `0xC0000005`, matching the
Tasks 246-248 unhandled-gate stdcall leak class.

The backend now maps valid Glide comparisons `0..7` to OpenGL
`GL_NEVER..GL_ALWAYS`. Every specialized-handler failure after return-address
and signature validation uses an ABI-preserving safe decline; only invalid
return addresses and signature mismatches remain hard rejects. Win32 x86 Debug
built successfully. A 30-second smoke run ended by normal timeout with exit 0,
progress 542,996, 49/49 handled Glide gates, three successful value-7 depth calls,
and zero reject, decline, OpenGL error, or caught exception. Direct value-3 live
confirmation still requires crossing the approximately 880-second frontier.

## Task 303: Glide 구현 공백 fatal 진단 / Glide implementation-gap fatal diagnostics

**확인됨.** 기존 코드는 catalog default와 safe decline만 전역 first-N debug로
출력했고, `_GRHINTS@8`, texture sampler 기본값, draw/LFB no-op 및 미지원
combine/blend retain은 구현 공백을 별도로 표시하지 않았습니다. 안전 반환은
`glide_gate_handled_count`에 포함되므로 기존 entries/handled 요약만으로는 이를
찾을 수 없었습니다.

플랫폼 공용 tracker가 이제 `GLIDE_UNIMPLEMENTED_FUNCTION`,
`GLIDE_UNSUPPORTED_ARGUMENT`, `GLIDE_BACKEND_FAILURE`, `GLIDE_ABI_REJECT`를
함수·인자 조합별로 기록합니다. 앞의 두 분류와 ABI reject는 fatal 진단이고 backend
실패는 error입니다. ABI가 검증된 구현 공백은 `action=continue`로 stdcall을 정상
정리하며, signature 미등록/불일치와 잘못된 반환 주소만 `action=terminate` hard
reject로 남습니다. 이 `fatal`은 구현 완성도 진단 등급이지 알려진 ABI 호출의 process
종료 정책이 아닙니다.

**검증됨.** 합성 probe는 중복 병합, 인자별 분리, 분류별 total, 128-record overflow와
공용 출력 문자열을 검증하고 exit 0으로 통과했습니다. Win32 x86 Debug loader/probe
빌드가 성공했습니다. 최종 30초 `pumpit1` smoke는 exit 0 정상 timeout,
progress 약 528,000, Glide gate 49/49였고 초기 구간에는 구현 공백 호출이 없어
total/unique/overflow가 모두 0이었습니다.

**미확정.** 현재 환경의 60초 smoke도 초기 49개 gate에 머물렀으므로 실제 콘텐츠
호출에서 즉시 line과 종료 summary가 같은 count를 갖는지는 장시간 실행에서 재확인해야
합니다. 다만 사용자 로그는 이후 `_GRTEXCLAMPMODE@12`,
`_GRTEXFILTERMODE@12`, `_GRTEXMIPMAPMODE@12`, `_GRTEXCOMBINE@28`,
`_GRHINTS@8`가 호출됨을 확인하므로 새 빌드에서는 해당 첫 조합마다
`[repiu-fatal]`이 출력됩니다.

**Confirmed.** The old code emitted only globally first-N debug lines for
catalog defaults and safe declines. Explicit `_GRHINTS@8`, texture-sampler
defaults, draw/LFB no-ops, and unsupported combine/blend retain paths were
silent. Safe returns also counted as handled, so entries/handled could not
identify implementation gaps.

The platform-neutral tracker now records `GLIDE_UNIMPLEMENTED_FUNCTION`,
`GLIDE_UNSUPPORTED_ARGUMENT`, `GLIDE_BACKEND_FAILURE`, and `GLIDE_ABI_REJECT`
by function/argument combination. The first two and ABI rejects are fatal
diagnostics; backend failures are errors. Verified-ABI gaps use
`action=continue` with normal stdcall cleanup, while uncataloged/mismatched
signatures and invalid return addresses remain `action=terminate` hard rejects.
Here fatal is implementation-completeness severity, not a process-termination
policy for known ABI calls.

**Verified.** The synthetic probe passed deduplication, argument separation,
per-category totals, 128-record overflow, and the shared exact log format with
exit 0. Win32 x86 Debug loader/probe builds succeeded. The final 30-second
`pumpit1` smoke ended by normal timeout with exit 0, progress about 528,000, and
49/49 Glide gates; no implementation-gap call occurred in that startup window,
so all issue totals remained zero.

**Unresolved.** This environment's 60-second smoke also remained at the first
49 gates, so agreement between immediate lines and final counts on real content
calls still needs a longer run. The user log does confirm later calls to the
texture sampler/combine no-ops and `_GRHINTS@8`; the rebuilt boundary will emit
`[repiu-fatal]` on each first unique combination.

## 텍스처 좌표 공간과 GrColor_t 형식 확정 (2026-07-28 Task 332) / Confirmed Texture Coordinate Space and GrColor_t Format

**확인됨:** 난이도 점이 몇 픽셀로만 보이던 근인은 **텍스처 좌표 정규화**였다.
Glide 좌표 공간은 텍셀 단위가 아니라 LOD와 무관하게 **긴 축이 항상 256**이고 짧은
축만 aspect ratio가 줄인다. 프레임 덤프에서 32×32 점 스프라이트(`largeLod=3`,
`aspect=1x1`)가 `st=(0,0)~(256,256)`으로, 64×64 화살표(`largeLod=2`)가
`st=(0,0)~(512,512)`로 제출되는 것을 직접 관측했다.

픽셀 크기로 정규화하면 긴 변이 256인 맵에서는 값이 같아 정상 동작하고, 그보다 작은
맵에서만 `256/크기` 배 초과가 되어 CLAMP로 quad의 그 비율만 덮인다. 40px quad에서
5px만 보이던 관측과 정확히 일치한다.

**확인됨:** 색상은 `grSstWinOpen`의 `cformat=1 = GR_COLORFORMAT_ABGR`이다.
`GrColor_t`를 ARGB로 읽으면 빨강과 파랑이 뒤바뀐다. 점의 상수색 `0xFE6565FE`는
ABGR로 빨강이며, ARGB로 읽어 파랗게 그려지던 것이 화면 관측과 일치했다.
게임이 쓰는 상수색은 대부분 무채색이라 이 결함을 가린다.

**확인됨:** `grTexTextureMemRequired`는 게스트 `GrTexInfo`를 읽지 않고 기본값(0)으로
계산해 **모든 텍스처에 8192바이트**를 답하고 있었다. 게스트는 그 답으로 자기 TMU
주소 공간을 배치하므로, 네 번의 계측에서 관측된 `0x2000` 균일 텍스처 간격은 원본
게임의 성질이 아니라 **우리 출력의 되먹임**이었다. 별도 결함으로 수정했으나 점
증상의 원인은 아니었다.

**기각됨:** `grDrawPoint`/`grDrawLine`/`grDrawPolygon`의 no-op 경로(게임이 호출하지
않음, 유일한 드로잉은 `grDrawTriangle`), 텍스처 미바인딩(`missing-sources=0`),
텍스처 디코드 불량(32×32 스프라이트 BMP·알파 정상), `chdir datas\texture` 실패
(원본 CHD에 없는 경로).

**Confirmed:** the difficulty dots rendered as a few pixels because Glide texture coordinates
are not texel units: the space is 256 along the longer axis whatever the LOD, with the shorter
axis scaled by the aspect ratio. A frame dump observed the 32x32 dot sprite submitted with st
spanning 0..256 and the 64x64 arrow with 0..512, so normalizing by pixel size overshoots by
`256 / size` for any map under 256 and clamps away the remainder. Colors follow
`GR_COLORFORMAT_ABGR` chosen at `grSstWinOpen`, so reading `GrColor_t` as ARGB swaps red and
blue — visible only on the dots' `0xFE6565FE` because the game's other constants are achromatic.
Separately, `grTexTextureMemRequired` never read the guest `GrTexInfo` and answered 8192 bytes
for every texture, making the observed uniform `0x2000` texture spacing a function of our own
output rather than evidence about the game. Rejected: the no-op point/line/polygon paths (never
called), missing texture bindings (no misses), texture decode faults (the sprite decodes
correctly), and the `datas\texture` chdir failure (absent from the original CHD).

## 배경 원근 좌표·table fog·LFB 재검증 (2026-07-30 Task 359) / Background Perspective, Table Fog, and LFB Revalidation

**확인됨:** 최신 frame dump에는 유효한 256×256 texture를 사용하는 전체 화면 크기
삼각형이 존재합니다. 따라서 Task 259의 “최대 232×39이고 배경 draw가 없다”는 결론은
당시 관측 구간에만 유효하며 현재 attract 경로에는 적용할 수 없습니다. asset 부재나
missing texture source가 현재 체크무늬 배경 결함의 주원인이라는 가설은 기각합니다.

**확인됨:** PIU의 60-byte producer에서 dword 8은 공용 `oow`, dword 9/10은 TMU0
`sow/tow`입니다. dword 11..14는 표본마다 정크를 포함해 가변하므로 유효한 TMU
reciprocal-w로 해석하지 않습니다. 공용 `oow`를 분자와 함께 보간하고 fragment에서
나누도록 바꾼 28초 smoke에서 전체 화면 texture quad의 12번째 triangle 뒤 back-buffer
비검정 픽셀은 `73,939/307,200`이었습니다. 이 결과는 background geometry와 texture가
실제 rasterize됨을 확인하지만, 사용자 원본 캡처와 픽셀 단위로 일치한다는 증명은
아닙니다.

**확인됨:** `_GRFOGMODE@4`는 초기 gate 표본에서 반복 호출되며 새 backend는 관측된
mode 0과 mode 2를 처리합니다. Task 359 smoke에는 `fog-mode-backend` 또는 unsupported
fog record가 없습니다. `grFogTable`과 `grFogColorValue`의 실제 attract 표본은 이번
짧은 구동에서 별도로 관측되지 않았으므로 table/color의 guest end-to-end 경로는
합성 fog knot/interpolation probe로 검증했습니다.

**확인됨:** LFB 565 staging은 세 번째 unlock에서 `19,224/614,400` non-zero bytes,
이후 `53,052/614,400`까지 증가합니다. 같은 RGBA 변환 결과의 BMP는 최신 표본에서
`30,858` nonblack pixels와 `(200,0)..(442,398)` bounding box를 가지므로 CPU decode가
검은 이미지를 만든다는 가설은 기각됩니다. 알려진 RGBA 표면을 직접 넣는
`repiu_glide_render_probe --opengl-lfb`도 shader bypass와 back-buffer readback을
통과합니다.

**확인됨:** 기존 `non-black` 진단은 RGB 채널이 8보다 큰 픽셀만 세어 fade 초기
LFB를 검정으로 오판했습니다. 세 번째 blit 입력은 `19,224`개 nonzero pixel과 최대
채널값 4를 가지며, 2배 drawable 출력은 정확히 4배인 `76,896`개 nonzero pixel과
동일 최대값 4를 가집니다. 네 번째 입력 `23,148`, 최대 8도 출력 `92,592`, 최대 8로
보존됩니다. viewport `1280×960`, program/texture binding, projection도 유효했습니다.
따라서 game-thread/host-thread LFB blit는 저휘도 입력을 실제 back buffer로 복사하며,
이전 `0/307,200`은 `>8` visibility threshold의 측정 해석 오류입니다.

**Confirmed:** the current frame dump contains full-screen triangles using a
valid 256x256 texture. This supersedes Task 259's observation that no draw was
larger than 232x39 for the currently reached attract path, rejecting missing
assets or texture sources as the primary background fault. PIU's 60-byte
producer uses dword 8 as shared `oow` and dwords 9/10 as TMU0 `sow/tow`;
dwords 11--14 remain variable and unconfirmed. With shared `oow` interpolated
and divided per fragment, the twelfth full-screen triangle leaves
73,939/307,200 nonblack back-buffer pixels. This proves rasterization, not
pixel-identical agreement with the reference capture.

Observed fog modes 0 and 2 no longer produce a fog backend/unsupported record.
The short smoke did not separately observe `grFogTable` or `grFogColorValue`,
so copied table/color end-to-end coverage remains synthetic through the knot,
interpolation, and clamp probe.

The third LFB unlock contains 19,224/614,400 nonzero staging bytes and later
unlocks reach 53,052/614,400. Its decoded BMP has 30,858 nonblack pixels in a
nonempty bounding box, and `repiu_glide_render_probe --opengl-lfb` proves the
dedicated shader bypass can copy a known RGBA surface to the back buffer. The
old `non-black` diagnostic counted only channels above 8 and therefore
misclassified the initial fade as black. The third real-game blit has 19,224
nonzero input pixels at maximum channel 4 and produces exactly 76,896 nonzero
pixels at maximum 4 in the 2x drawable. The fourth maps 23,148 pixels at
maximum 8 to 92,592 at maximum 8. Real game-thread/host-thread LFB copying is
therefore confirmed; the earlier 0/307,200 value was a threshold-interpretation
error.

## LFB ABGR/BGR565 채널 순서 확인 (2026-07-30 Task 360) / LFB ABGR/BGR565 Channel Order

**확인됨:** PIU는 `grSstWinOpen`의 `cFormat`과 `grLfbWriteColorFormat`에 모두
`1`(`GR_COLORFORMAT_ABGR`)을 전달하고, 문제가 발생한 장면은 write-only
`grLfbLock(..., GR_LFBWRITEMODE_565, ...)`으로 기록합니다. 일반 texture가 정상이고
이 장면만 Blue/Cyan에서 Yellow로 바뀐다는 사용자 관측은 전역 texture decode가 아니라
LFB Red/Blue 교환으로 범위를 한정합니다.

**확인됨:** Glide 2.4 Programming Guide Table 11.2에서 ABGR/BGRA의 565 LFB 배치는
bits 15..11 Blue, bits 10..5 Green, bits 4..0 Red입니다. 기존 HLE는 이를 항상
RGB565로 decode/encode했습니다. 같은 `0xF800`은 RGB 순서에서 Red이지만 ABGR/BGR
순서에서는 Blue이며, Cyan은 BGR565 `0xFFE0`입니다.

**검증됨:** color-format 인식 변환 probe에서 RGB/BGR pure red/blue와 Cyan
encode/decode 왕복이 통과했습니다. 35초 실제 `aot-dbt` smoke는
`grLfbWriteColorFormat(1)` 뒤 565 lock/unlock을 반복했고 fatal/backend failure 없이
supervisor 제한으로 종료됐습니다. 수정 후 dump 245의 전체 픽셀 통계는 평균
RGB `11.45/74.36/82.47`, Blue/Cyan 우세 `115,200`, Yellow 우세 `0`이며 대표색
`(33,251,255)`입니다. 따라서 LFB 채널 교환 원인과 수정 효과가 실제 guest write
경로에서 확인됐습니다.

**Confirmed:** PIU passes `GR_COLORFORMAT_ABGR` to both `grSstWinOpen` and
`grLfbWriteColorFormat`, and the affected scene uses a write-only 565 LFB lock.
Glide 2.4 Table 11.2 defines that lock as BGR565 (Blue high, Red low), while
the old HLE always used RGB565. Synthetic red/blue and cyan round trips pass
after making conversion color-format aware. A 35-second real-game smoke
repeated the format-1 565 lock/unlock path without fatal/backend failure. Full
statistics for corrected dump 245 are mean RGB `11.45/74.36/82.47`, 115,200
blue/cyan-dominant pixels, zero yellow-dominant pixels, and dominant color
`(33,251,255)`, confirming the fix on guest-written data.

## 상태 setter 반복률과 rendezvous 후 첫 GL 접촉 비용 (2026-07-30 Task 364) / Setter Repetition and Post-Rendezvous First-GL-Touch Cost

**확인됨:** 동일 바이너리 Release 60초 3회에서 상태 setter 호출의 **90.71%가 직전
성공 적용과 정확히 같은 상태**입니다(범위 90.65~90.72%). census 대상 20종 중 13종이
99%를 넘습니다.

| ordinal | API | 호출(중앙값) | 반복률 | 최대 연속 | 고유 인수 |
|---:|---|---:|---:|---:|---:|
| 91 | `grColorMask` | 6,161 | 99.95% | 6,158 | 1 |
| 92 | `grConstantColorValue` | 6,146 | 77.67% | 456 | 8+ |
| 98 | `grDepthMask` | 5,418 | 72.63% | 9 | 2 |
| 138 | `grTexSource` | 5,099 | 32.24% | 3 | 8+ |
| 131/134/136 | `grTexClampMode`/`FilterMode`/`MipMapMode` | 5,099 | 99.73% | 1,382 | 8+ |
| 101 | `grFogMode` | 4,679 | 99.96% | 4,676 | 1 |
| 89 | `grClipWindow` | 4,674 | 99.96% | 4,672 | 1 |
| 79 | `grAlphaBlendFunction` | 4,674 | 99.94% | 4,669 | 2 |

failure 0, unsupported 0, key overflow 0입니다. 즉 모든 setter 호출이 host에
성공적으로 적용되며, 반복률은 게임이 프레임마다 같은 상태를 다시 설정한다는 사실을
그대로 나타냅니다.

**확인됨: host work의 지배 항목은 특정 GL 함수가 아니라 rendezvous 기상 직후의 첫 GL
접촉입니다.**

| setter | GL phase | 중앙값 |
|---|---|---:|
| `grDepthMask` | `glDepthMask` (첫 호출) | **84.59%** |
| `grDepthMask` | 후속 `glGetError` | 15.41% |
| `grAlphaBlendFunction` | 선행 `glGetError` (첫 호출, **반복 0회**) | **30.66%** |
| `grAlphaBlendFunction` | `glEnable`+`glBlendFunc` | 67.13% |
| `grAlphaBlendFunction` | 후속 `glGetError` | 2.21% |

alpha blend의 선행 drain loop는 세 실행 모두 반복 0회이므로 그 30.66%는
`GL_NO_ERROR`를 즉시 반환하는 `glGetError` **한 번**의 비용입니다. 같은 함수 안의
후속 `glGetError`는 2.21%뿐이므로 **동일한 호출이 위치에 따라 약 14배** 차이가 납니다.
비용은 host thread가 condition variable에서 깨어난 뒤 처음 GL을 만지는 지점에 있습니다.

> **[Task 369에서 뒤집힘] 위 문단에 있던 "`glGetError`를 전역적으로 제거하는 방향은
> 기각된다"는 결론은 철회합니다.** 측정 자체는 옳았으나 **자동 부팅 장면에만**
> 해당했습니다. 사용자가 캡처한 실제 gameplay 장면에서 `grDepthMask`의 후속
> `glGetError`는 15.41%가 아니라 **99.81%**였고, 호출당 491,356 cycle(wall의 6.24%)
> 이었습니다. 같은 호출이 자동 장면에서는 4,600 cycle이므로 **장면 간 107배**
> 차이입니다.
>
> 이는 "위치에 따라 14배"라는 관측과 모순되지 않고 오히려 같은 메커니즘의 연장입니다
> — `glGetError`는 동기화 지점이므로 비용이 **앞에 쌓인 명령량에 비례**하며, 삼각형
> 배치 뒤에 오는 gameplay 장면의 호출이 가장 비쌉니다. 자동 장면의 15.41%를 근거로
> 전역 기각을 결론지은 것이 오류였습니다.
>
> 상세: [glide-gate-cost-attribution.md](glide-gate-cost-attribution.md),
> [설계 369](../design/20260731-369-glide-gl-error-check-policy.md)

**확인됨:** 계측한 GL 구간은 ordinal host work의 **58.53~67.26%**만 덮습니다. 나머지
32.74~41.47%는 `is_open` 검사, `message_` 대입, lambda·dispatch 비용입니다.

**확인됨(장면 의존성):** 동일 상태 생략의 실측 상한은 wall의 4.55%, Glide gate의
25.11%입니다. 부팅 포함 60초 실행은 `grLfbLock` 304회를 포함해 그 gate가 Glide를
지배하므로 같은 setter 집합이 wall의 약 5.57%뿐입니다. Task 363의 LFB 없는 gameplay
장면에서는 20.59%였습니다. **wall 기준 setter 판정은 장면 구성에 좌우되므로 Glide
gate 대비 값을 장면 간 비교 축으로 씁니다.**

**미확정:** GL 밖 host work 32.74~41.47%의 내부 구성. 파생 커널 전이 추정이 control
6.80%에서 profile 14.60%로 움직인 원인(프레임 변화는 -2.79%뿐).

[작업 로그](../work-logs/20260730-364-glide-setter-state-census.md)

**Confirmed:** Across three same-binary 60-second Release runs, 90.71% of state
setter calls exactly repeat the previously applied state, with thirteen of the
twenty census setters above 99%, `grColorMask` highest at 99.95% over a longest
run of 6,158, `grDepthMask` at 72.63%, and `grTexSource` lowest at 32.24%. There
were no failures, unsupported arguments, or key overflows, so every setter call
lands on the host and the repetition rate reflects the game reprogramming the
same state each frame.

**Confirmed:** The dominant host cost is the first GL touch after the rendezvous
wake rather than any particular GL function. `glDepthMask` holds 84.59% of the
depth-mask OpenGL interval against 15.41% for the trailing `glGetError`, and the
alpha-blend leading drain holds 30.66% while iterating zero times in all three
runs — the cost of a single `glGetError` returning `GL_NO_ERROR` — against 2.21%
for the identical call later in the same function, roughly a fourteen-fold
difference by position alone.

> **[Overturned by Task 369]** The conclusion previously drawn here — that
> removing `glGetError` globally is rejected as a direction — is withdrawn. The
> measurement was right but described the automated boot scene only. In the user's
> gameplay captures the trailing `glGetError` on `grDepthMask` is **99.81%**, not
> 15.41%, at 491,356 cycles per call and 6.24% of wall time; the same call costs
> 4,600 cycles in the automated scene, a **107-fold** difference by scene alone.
> This does not contradict the fourteen-fold positional observation — it extends
> the same mechanism, since `glGetError` is a synchronisation point whose cost
> tracks what is queued ahead of it, and the gameplay calls follow a triangle
> batch. Generalising a global rejection from the automated scene was the error.
> See [glide-gate-cost-attribution.md](glide-gate-cost-attribution.md).

**Confirmed:** The instrumented OpenGL interval covers only 58.53-67.26% of the
ordinal's host work; the remaining 32.74-41.47% is the open check, the message
assignment, and dispatch.

**Confirmed:** The exact-duplicate elision ceiling is 4.55% of wall time and
25.11% of the Glide gate. Because the boot-inclusive run includes 304
`grLfbLock` calls whose gate dominates Glide, the same setter set holds only
about 5.57% of wall here against 20.59% in the Task 363 gameplay capture, so the
gate-relative figure is the axis comparable across scenes.

**Unresolved:** the composition of the non-GL host work, and why the derived
kernel-transition estimate moved from 6.80% to 14.60% while frames moved only
-2.79%.

## 동일 상태 setter의 rendezvous 생략과 그 한계 (2026-07-30 Task 365) / Eliding Repeated Setter State and Its Limit

**확인됨:** 반복률 99.9% 이상인 7종(`grColorMask`, `grAlphaBlendFunction`,
`grClipWindow`, `grAlphaTestFunction`, `grFogMode`, `grCullMode`,
`grDepthBufferFunction`)에서 정확한 동일 상태의 host rendezvous를 생략했습니다. 기본
ON이며 `REPIU_GLIDE_SETTER_ELIDE=0`으로 복원합니다.

**확인됨(호출 구조):** 이 장면에서 게임은 60초 동안 이 7종을 41,384회 호출해 상태를
**16번** 바꿉니다. 비율 약 **2,586 : 1** 이며 `applied=16`은 3회 실행 모두 동일했습니다.

**확인됨(정확성):** 동작을 바꾸지 않는 census가 센 중복과 실제 생략이 ordinal 단위와
합계 모두 정확히 일치했습니다.

| ordinal | API | calls | census `same` | cache `elided` |
|---:|---|---:|---:|---:|
| 91 | `grColorMask` | 7,461 | 7,458 | 7,458 |
| 101 | `grFogMode` | 5,663 | 5,661 | 5,661 |
| 82 | `grAlphaTestFunction` | 5,656 | 5,654 | 5,654 |
| 89 | `grClipWindow` | 5,656 | 5,654 | 5,654 |
| 94 | `grCullMode` | 5,656 | 5,654 | 5,654 |
| 79 | `grAlphaBlendFunction` | 5,656 | 5,653 | 5,653 |
| 96 | `grDepthBufferFunction` | 5,656 | 5,653 | 5,653 |

**확인됨(렌더 동일성):** swap별 back-buffer 통계를 phase offset을 주고 대응시키면
offset +1에서 **72.9%가 완전 일치**하고(non-black 픽셀 수와 R/G/B 평균 전부) 다른
offset은 0.0~15.9%입니다. 같은 프레임을 같은 순서로 한 프레임 먼저 그립니다. mask나
blend가 깨졌다면 어떤 offset에서도 일치하지 않습니다.

**확인됨(생략이 안전한 세 근거):**
1. 캐시 상태를 setter 밖에서 바꾸는 유일한 경로인 LFB blit은 `glIsEnabled`·
   `glGetBooleanv`로 실제 GL 상태를 조회해 저장하고 복원하며, `glDepthMask`·
   `glBlendFunc`·`glDepthFunc`·fog·dither·scissor box는 건드리지 않습니다.
2. `glide_state` mirror 쓰기 생략은 멱등합니다. 이 mirror는 `grGlideGetState`가
   `BuildGlideStateImage`로 guest에 돌려주므로 실제로 읽히지만, key가 인수 dword
   전체를 담으므로 직전 적용이 이미 동일한 값을 썼습니다.
3. 생략은 반환 주소·signature·인수 크기 검증을 모두 통과한 뒤에만 적용됩니다.

**확인됨(한계):** Glide gate 비중은 `20.76% → 15.63%`(-5.13%p)로 내려갔지만 프레임
중앙값은 `1,215 → 1,206`(-0.74%)으로 변하지 않았습니다. OFF 3회 범위가
1,215~1,384이므로 편차 안입니다. **이 장면의 실행은 더 이상 Glide setter 경로에 의해
제한되지 않습니다.** 해방된 시간은 guest 실행 추정 `62.17% → 63.12%`와 커널 전이 추정
`7.26% → 10.08%`로 흘러갔습니다.

**미확정:** 무엇이 pacing하는지. 커널 전이 추정이 2.8%p 오른 이유. LFB 없는 gameplay
장면(Task 363 기준 setter가 Glide gate의 85.33%)에서의 이득.

**방법 메모:** back buffer 스크린샷 기능은 현재 없습니다. `REPIU_GLIDE_FRAME_DUMP`는
이미지가 아니라 draw-call 추적이고, `build/texture_dumps/`의 BMP는 텍스처 dump입니다.
따라서 cross-run 시각 검증은 `REPIU_GLIDE_PIXEL_DIAG`의 swap별 통계를 phase offset으로
대응시키는 방식을 씁니다.

[작업 로그](../work-logs/20260730-365-glide-setter-state-elision.md)

**Confirmed:** The seven setters repeating at 99.9% or better now skip the host
rendezvous when the arguments exactly match a state already applied
successfully. In this scene the game issues 41,384 such calls per 60 seconds in
order to change state 16 times, about 2,586 to 1, with `applied` reading exactly
16 in all three runs.

**Confirmed:** The behaviour-neutral census and the actual elision agreed exactly,
per ordinal and in aggregate, so only observed duplicates were skipped. Matching
per-swap back-buffer statistics under a phase offset gives 72.9% exact identity
at +1 — non-black pixel count and all three channel means — against 0.0-15.9% at
every other offset, so the same frames are drawn in the same order one frame
sooner. A broken mask or blend would match at no offset.

**Confirmed:** Three things make the elision unobservable. The LFB blit, the only
path that mutates cached state outside the setters, saves it by querying real GL
state and restores it, and never touches depth mask, blend func, depth func, fog,
dither, or the scissor box. Skipping the `glide_state` mirror write is idempotent
— it really is read back through `BuildGlideStateImage`, but the key holds every
argument dword so the matching application already wrote identical values. And
the decision runs only after return-address, signature, and argument-size
validation pass.

**Confirmed limit:** the Glide gate share fell from 20.76% to 15.63% while median
frames stayed at 1,215 to 1,206, inside the elide-off range of 1,215-1,384.
Execution in this scene is no longer limited by the Glide setter path, and the
freed time went into the guest-execution and kernel-transition estimates.

**Unresolved:** what paces the run, why the kernel estimate rose 2.8 points, and
the gain in an LFB-free gameplay scene.

**Method note:** there is no back-buffer screenshot facility.
`REPIU_GLIDE_FRAME_DUMP` is a draw-call trace, and the BMPs under
`build/texture_dumps/` are texture dumps, so cross-run visual verification uses
phase-offset matching of `REPIU_GLIDE_PIXEL_DIAG`'s per-swap statistics.

## Gameplay의 line과 cull mode 1 관측 (2026-07-31 Task 374) / Gameplay line and cull-mode-1 observation

**확인됨:** `gameplay-capture.log`에서 `grDrawLine`은 네 endpoint 조합으로 총
11,024회 호출됩니다. 포인터 네 개는 60바이트 간격이고 각 조합은 2,756회 반복되어
닫힌 사각 테두리를 만듭니다. 따라서 Task 332의 line 미호출 결론은 당시 자동 장면에만
유효하며 gameplay 전체에는 일반화할 수 없습니다.

**확인됨:** 같은 장면은 `grCullMode(1)`을 한 번 요청합니다. 값 1은 원 사양의
`GR_CULL_NEGATIVE`이며 기존 backend가 mode 0만 받아 거부했습니다. 이후 23,540회 cull
호출 중 나머지는 기존 지원/생략 경로였습니다.

**구현됨:** 공용 60바이트 decoder를 line과 triangle이 함께 사용하고, line은 같은
OpenGL state 제출 경로의 1픽셀 `GL_LINES`로 연결했습니다. cull 0/1/2는 현재 origin을
포함해 OpenGL face로 변환합니다.

**미확정:** 원본 Voodoo 출력과의 픽셀 단위 line rasterization 차이. 제공된 장면은 사용자
입력으로 service/test 화면에 진입하므로 이번 자동 probe만으로 live 재현되지 않습니다.

**Confirmed:** The gameplay capture contains 11,024 `grDrawLine` calls across
four endpoint pairs. Four pointers at 60-byte intervals form a closed outline,
each pair repeated 2,756 times. Task 332's no-line finding was therefore specific
to its automated scene rather than gameplay in general.

The same scene requests `grCullMode(1)` once. Mode 1 is documented
`GR_CULL_NEGATIVE`; the old mode-zero-only backend rejected it. Shared 60-byte
decoding now feeds both line and triangle paths, lines use one-pixel `GL_LINES`,
and cull modes 0 through 2 translate by current origin. Pixel-level agreement
with Voodoo line rasterization and an input-driven live replay remain unresolved.


## Task 375 (2026-08-01): 텍스처 축은 결백하다 — **확인됨**

music select의 fps를 두고 텍스처를 의심해 census와 픽셀 덤프를 만들어 판정했습니다.

music select 24.1초 / 793프레임:

| 지표 | 값 |
|---|---:|
| uploads / distinct | 41 / 29 |
| **동일 내용 재업로드** | **0** |
| **디코드 실패** | **0** |
| **팔레트 없는 팔레트 텍스처** | **0** |
| 8비트 포맷(0/2/3/4/5/14) | **0** — 전부 16비트 |
| extent 불일치 | 3 (8·32·64 LOD, Task 332의 정상) |
| 업로드 빈도 | 프레임당 **0.05회**, 0.4 MB/s |
| 포맷 분포 | RGB_565 5건 / ARGB_4444 36건 |

픽셀 육안 확인도 통과했습니다 — ARGB_4444 발판은 색과 알파(43% 투명)가 정확하고,
RGB_565 하트는 빨강/초록이 정상이며, 64×256 배너는 종횡비 5로 올바르게 세로입니다.

**오진 기록:** 자동 장면의 fmt10 두 장이 청록이라 적색 채널 손실을 의심했으나
반증됐습니다. 채널 값 분포 22/53/26이 5/6/5 비트와 일치하고, R/B 교환은 노란색이
되며, 같은 포맷이 다른 장면에서 정상 색을 냅니다. 색보정된 BGA 아트였습니다.

**8비트 경로는 구현되어 있으나 이 게임이 쓰지 않습니다.** 디코더와
`IsGlideTextureFormatAcceptable`이 RGB_332·ALPHA_8·INTENSITY_8·ALPHA_INTENSITY_44·
P_8·AP_88을 지원하고 `_GRTEXDOWNLOADTABLE@12`로 팔레트도 받습니다. 거부되는 것은
NCC 압축인 포맷 1(YIQ_422)과 9(AYIQ_8422)뿐입니다.

**중복 정리:** 기존 `REPIU_DUMP_TEXTURE_BMP` 경로를 제거했습니다. 24비트라 알파를
잃었고(기존 주석이 "조사 중인 바로 그 경우"라고 인정), 덤프 전용으로 텍스처를 한 번
더 디코드하고 있었습니다. `DumpTextureToBmp`는 LFB 전용이 되어
`DumpLfbSurfaceToBmp`로 개명됐습니다. `REPIU_DUMP_LFB_BMP`는 같은 알파 손실을
안은 채 남아 있어 향후 통합 후보입니다.

Task 375 settled a suspicion about textures by building the inspection that did not exist. Over 24.1
seconds of music select the census recorded 41 uploads against 29 distinct addresses with zero
identical repeats, zero decode failures, zero palettized uploads missing a palette, and no 8-bit
formats at all — 0.05 uploads per frame at 0.4 MB/s. Dumped pixels confirm correct colour and alpha
in both formats present. A mid-investigation suspicion that RGB_565 was losing its red channel was
disproved three ways, the cyan being colour-graded BGA artwork. The 8-bit paths including P_8 and
its palette download exist and work; this game simply does not use them. The pre-existing
`REPIU_DUMP_TEXTURE_BMP` dump was removed as a duplicate that dropped alpha and decoded every
texture twice, leaving its writer to serve the LFB surface alone.


## GLSL 셰이더 구조와 남은 `glGetError` (2026-08-01 검토) — **확인됨 / 미해결**

### 구조 — 확인됨

Glide의 color/alpha combine, fog, 텍스처 샘플링을 구현한 **단일 GLSL 프로그램**
입니다. `Initialize()`는 창 생성 시 **1회**만 호출되고 `compile_shader` /
`link_program`은 그 안에서만 쓰입니다. **런타임 재컴파일이 없으므로 셰이더 컴파일
스톨은 이 설계에 존재할 수 없습니다.** `glBegin`/`glEnd` 즉시 모드 위에 얹혀
있습니다.

진입점은 여덟 개이고 그중 `SetTextureEnabled`만 draw마다 호출됩니다.

### 남은 `glGetError` 5개 — 미해결, wall의 8.90%

Task 369가 `glide_opengl_backend.cpp`만 정책 게이트 뒤로 옮기고 셰이더 모듈은
건드리지 않았습니다. `SetFogMode`(437) / `SetFogColor`(458) / `SetFogTable`(482) /
`SetAlphaCombine`(508) / `SetColorCombine`(540)에 남아 있습니다.

| ordinal | 셰이더 `glGetError` | work/call | wall |
|---|---|---:|---:|
| **`grAlphaCombine`** | **있음** | **285,694** | **6.62%** |
| `grFogColorValue` | 있음 | 168,061 | 1.26% |
| `grColorCombine` | 있음 | 32,823 | 0.76% |
| `grFogTable` | 있음 | 35,261 | 0.26% |
| **`grConstantColorValue`** | **없음** | **1,676** | 0.06% |

**같은 파일·같은 uniform 업로드 기구인데 유무로 170배 차이**입니다. uniform 업로드
자체는 싸고 비용은 전부 에러 체크입니다. `grAlphaCombine`이 369 이전 48,980 →
285,694로 5.8배 오른 것은 `grDepthMask`가 배수를 멈추자 누적 명령이 다음
`glGetError`인 이곳에서 배수되기 때문입니다.

**부수:** `SetTextureEnabled`가 draw마다 조건 없이 `glUseProgram` + `glUniform1i`를
부릅니다. 약 0.06 ms/frame으로 게이트 미달입니다.

진행 방향은 [설계 377](../design/20260801-377-shader-gl-error-check-completion.md).

Reviewed 2026-08-01. The shader is a single GLSL program covering Glide's colour and alpha combine,
fog, and texture sampling, initialised once at window creation with no runtime recompilation, so
compile stalls cannot occur here. Task 369 gated the backend's error checks but left five in the
shader module, measured at 8.90% of wall on a gameplay capture with `grAlphaCombine` alone at
285,694 cycles per call. The same file's `SetConstantColor` has no check and costs 1,676 through
identical machinery -- a 170-fold gap showing the uniform uploads are cheap and the cost is the
check. Direction recorded in design 377.


## 371초 gameplay 전수 호출 census와 깊이 확인 (2026-08-07 Tasks 433·437) — **확인됨**

사용자 실행 로그 하나(v0.0.136, pumpit1, 01:39–01:45, **371.3초 / 20,212프레임 =
54.4 fps**)에서 확정된 것들입니다.

**깊이는 정상입니다.** Task 433의 `ooz` 연결 이후 gameplay 3D 모델이 정상으로 보인다고
사용자가 확인했습니다. 같은 실행에서 Glide 구현 공백은
`unimplemented/unsupported/backend/abi/unique/overflow = 0/0/0/0/0/0`이고, 게이트
5,586,761건이 전건 처리(미처리 0)됐습니다.

**게이트 호출 구성 — 4분의 3이 setter입니다.**

| 구분 | 호출 | 비중 | 프레임당 |
|---|---:|---:|---:|
| 게이트 크로싱 전체 | 5,586,761 | 100% | 276.4 |
| `grDrawTriangle` | 1,388,559 | 24.9% | 68.7 |
| setter 계열 | 약 4,150,000 | 74.3% | 205.3 |

| ordinal | 호출 | 프레임당 | 비고 |
|---|---:|---:|---|
| `grTexSource`·`grTexClampMode`·`grTexFilterMode`·`grTexMipMapMode` | **각 395,764** | 각 19.6 | 네 값이 **정확히 동일** — bind당 4-call 블록, bind당 삼각형 3.5개 |
| `grColorMask` | 327,042 | 16.2 | batch 1 |
| `grDepthMask` | 326,884 | 16.2 | 미생략 |
| `grFogMode` | 288,167 | 14.3 | batch 1 |
| `grDepthBufferFunction`·`grCullMode`·`grClipWindow`·`grAlphaTestFunction`·`grAlphaBlendFunction` | 각 286,715 | 각 14.2 | batch 1 |
| `grConstantColorValue` | 72,710 | 3.6 | 미생략 |
| `grDitherMode`·`grColorCombine`·`grAlphaCombine`·`grHints` | 약 26,440 | 1.3 | — |
| `grBufferSwap`·`grBufferNumPending`·`grBufferClear` | 20,212 / 20,212 / 20,163 | 1.0 | 프레임 경계 |
| `grTexDownloadMipMapLevel` | 176 | 0.009 | 텍스처 다운로드 |

**Task 365 생략은 사실상 완전 적중입니다.** batch 1 7종에서
`elided 2,048,762 / applied 22` — **99.999%** 가 같은 값의 반복입니다. 무효화는 4회뿐.

**게이트 예외는 0입니다.** direct dispatch가 `entry/success = 5,586,761/5,586,761`,
target-miss 0, terminal 0. 크로싱당 남은 비용은 host rendezvous입니다.

**텍스처 census.** 업로드 176 / distinct **62** / 동일 재업로드 3 / 내용 변경 재업로드
111, 디코드 실패 0, 포맷 10이 63건·포맷 12가 113건, 긴 변 256이 166건. **"PTX 465개 중
4개만 도달"이라는 옛 요약은 낡았습니다** — 이 실행만으로 distinct 62입니다.

**timer tick.** due 88,682 / injected 88,652 / **coalesced 0** / dropped 30 /
max-backlog 13 — Task 432가 gameplay 장시간 실행에서도 유지됩니다.

Confirmed on 2026-08-07 from a single user run of v0.0.136 (pumpit1, 371.3 seconds, 20,212
frames, 54.4 fps). **Depth is correct**: after Task 433 connected `ooz`, the user confirms the
gameplay 3D models look right, and the same run reports zero Glide implementation gaps with all
5,586,761 gate crossings handled. **Three quarters of those crossings are state setters** —
1,388,559 draws (24.9%) against roughly 4.15M setters (74.3%), or 68.7 draws and 205 setters per
frame. The texture-state block stands out: `grTexSource`, `grTexClampMode`, `grTexFilterMode` and
`grTexMipMapMode` are called **exactly 395,764 times each**, one four-call block per bind with 3.5
triangles per bind. **Task 365's elision is essentially a total hit** — 2,048,762 elided against
22 applied across its seven gates, 99.999%, with four invalidations. **The per-call gate exception
is gone**: direct dispatch reports 5,586,761 of 5,586,761 with no target miss, so the remaining
per-crossing cost is the host rendezvous. The texture census records 176 uploads over 62 distinct
addresses with zero decode failures, which **retires the stale summary that only four of 465 PTX
assets ever reached Glide**. Timer ticks hold at 88,652 injected of 88,682 due with zero
coalesced, so Task 432 survives a long gameplay run.


## 실부하 gameplay의 크로싱 구성 — draw가 69.9%다 (2026-08-07 Task 437 A/B) — **확인됨**

Task 437 A/B 4회(각 27.8~81.5초, pumpit1)에서 **371초 실행의 구성이 대표값이 아님**이
드러났습니다. 그 실행은 메뉴가 섞여 draw가 프레임당 68.7이었지만, 실부하 구간은
**652~686**로 10배입니다.

| 프레임당(실측, elision batch 2 ON) | 회 | 비고 |
|---|---:|---|
| 게이트 크로싱 | 959.4 | — |
| **`grDrawTriangle`** | **670.8 (69.9%)** | `DrawTriangle`은 **삼각형당 rendezvous 1회** (`glide_opengl_backend.cpp:1152-1159`) |
| 생략된 setter | 221.3 | rendezvous 없음 |
| 미적용 setter | 65.5 | `grTexSource` 32.1 · `grDepthMask` 18.6 · 그 밖 |
| 적용된 setter | 1.8 | — |

**Task 365/437 생략은 정확성이 확정됐습니다.** 네 실행 모두 `voided=0`, 구현 공백 0,
그리고 **대상 호출 = elided + applied가 오차 0으로 닫힙니다.** batch 2가 덮은 텍스처
setter 385,197건 중 실제 적용은 **933건(0.24%)** 뿐이고, 사용자 육안으로도 차이가
없었습니다.

**프레임 효과는 미판정입니다.** 이 A/B는 vsync가 켜진 상태였고(`swap interval override
requested=false`) time profile·census도 꺼져 있어, 가이드가 요구한 측정 조건을
충족하지 못했습니다. fps 차 −1.9%는 편차와 구분되지 않습니다.

Confirmed on 2026-08-07 from the Task 437 A/B. The 371-second run's composition was **not
representative**: a real load section carries **652-686 draws per frame against that run's 68.7**,
putting `grDrawTriangle` at **69.9% of gate crossings**, and `DrawTriangle` costs **one host
rendezvous per triangle**. Per frame that is 670.8 draw rendezvous against 65.5 for uncovered
setters and 1.8 applied, with 221.3 elided costing nothing. The elision's **correctness is
settled** — zero `voided`, zero implementation gaps, covered calls equal elided plus applied with
no remainder in all four runs, only 933 of 385,197 newly covered texture-setter calls actually
applied, and no visual difference reported. Its **frame effect remains unmeasured**, because
vsync was on and neither the time profile nor the census was enabled.


## draw batching A/B — 게이트 비중 10.35% → 8.40% (2026-08-07 Tasks 438·439) — **확인됨**

사용자 짝 실행(Release, `swap interval effective 0`, time profile ON, pumpit1 gameplay).

| 지표 | batch `=0` | batch `=1` |
|---|---:|---:|
| 실행 시간 / 프레임 | 80.6초 / 5,973 | 75.1초 / 7,276 |
| draw | 2,831,099 (474.0/frame) | 2,677,699 (368.0/frame) |
| **mean-batch (프리미티브/flush)** | — | **16.02** (최대 **332**) |
| flush | — | 167,133 — **전부 non-draw-gate**(용량·primitive 전환 0) |
| **glide-gate ÷ guest-run** | **10.35%** | **8.40%** |
| 게이트 크로싱당 | 7,334 cycle | **5,598** (−23.7%) |
| draw 1회당 총 게이트 | 10,883 cycle | **8,694** (−20.1%) |
| ordinal 73 호출당 / `rendezvous` | 6,969 / 2,831,099 | **2,239 / 0** |
| `failures`·`voided`·구현 공백 | 0 | 0 |

**왕복 1회의 단가(확인됨).** 배치 없는 실행에서 `_GRDRAWTRIANGLE@12`는 호출당
**6,969~7,373 cycle**이고 그중 queue 518~566 · wake 1,673~1,957 · **work(실제 GL)
912~948** · complete 1,246~1,295입니다. **GL 작업은 13% 안팎이고 왕복이 절반**입니다.

**비용의 일부는 사라지고 일부는 옮겨갑니다.** draw ordinal에서 빠진 4,730 cycle/draw 중
약 2,541은 flush를 유발한 ordinal로 이동하고(`grBufferSwap`의 `rendezvous`가 7,276
호출에 14,386) **약 2,189가 실제로 사라집니다** — 총 5.86e9 cycle = guest-run의 2.1%.

**배치 길이 예측은 빗나갔고, 실측이 훨씬 좋습니다.** "draw ÷ flush 지점 = 5.44"는 모든
flush 지점에 그릴 것이 있다고 가정한 값입니다. 실제로는 draw가 최대 332개까지 연속으로
뭉치고 flush 지점 대부분이 빈 큐를 만나 **평균 16.02**가 나왔습니다.

**attract 구간은 이 축을 판정할 수 없습니다.** 프레임당 draw가 7개이고 게임이 사각형
단위로 그려 배치가 **항상 2**이며, 그 구간 A/B에서는 총 게이트 비용이 줄지 않았습니다.

**프레임은 판정 지표가 아닙니다.** 같은 구성으로 돌린 두 실행이 **13%** 차이 난 적이
있고(81.1 대 91.8 fps), 기대 효과는 2~3%입니다. 구간 길이에 둔감한
`glide-gate ÷ guest-run` 비중을 씁니다.

Confirmed on 2026-08-07 from the user's paired gameplay A/B on Release with vsync disabled and the
time profile on. Batches averaged **16.02 primitives** with a peak of 332, every one of the 167,133
flushes coming from the non-draw-gate rule, and the **Glide gate fell from 10.35% to 8.40% of
guest-run** — 23.7% per crossing, 20.1% per draw — with zero failures, zero voided setters, zero
implementation gaps and no visual difference. Without batching a draw gate costs **6,969-7,373
cycles**, of which the actual GL work is only **912-948** and the round trip is about half; of the
4,730 cycles per draw that leave the draw ordinal, roughly 2,541 reappear at the flush sites and
**2,189 genuinely disappear**, 2.1% of guest-run. The predicted batch length of 5.44 assumed every
flush point had work pending; draws actually cluster far longer, up to 332 consecutively. The
attract phase cannot judge this axis at all — seven draws per frame in quads pin the batch at two,
and total gate cost did not move there. Frames are not the metric: two runs of one configuration
have differed by 13% while the effect is 2-3%, so the scene-insensitive gate share is what decides.

## pumpit8 LFB region 전면 화면 전송 (2026-08-13 Task 476) — **확인됨**

`pumpit8`은 `grLfbReadRegion`(ordinal 98)과 `grLfbWriteRegion`(ordinal 99)을 **각각
1440회** 호출합니다(Glide call trace 기준). `1440 = 3 x 480`이므로 **480행 전면 화면
전송을 3회** 수행한 것입니다. 한 호출은 `width = 640`, `height = 1`, `stride = 0`,
buffer = BACK이고 `y`는 `0x1DF`(479)에서 1씩 내려갑니다. read의 목적지와 write의
원본은 서로 다른 guest 버퍼입니다(`0x051C1B28` 대 `0x051C40F8`).

> **정정.** 이 관측을 처음 정리할 때 "하단 128행"이라고 적었으나 오류였습니다. 128은
> `kGlideImplementationIssueRecordCapacity`(구현 공백 기록 테이블 용량)이며, 서로 다른
> 인자 조합이 128개에서 잘린 것을 행 수로 오해한 것입니다. **issue 기록 건수를 호출
> 횟수로 읽으면 안 됩니다.** 실제 호출 횟수는 Glide call trace의 `count`에 있습니다.

`grLfbWriteRegion`의 `src_format`은 **`5` = `GR_LFB_SRC_FMT_8888`** 입니다. read는
포맷 인자가 없으므로 프레임 버퍼 native 565를 돌려주어야 합니다. 즉 게스트는 565를
읽어 8888로 가공한 뒤 되쓰는 full-screen 합성 경로입니다.

### 관련 상태값 (같은 로그에서 확인)

| 호출 | 인자 | 값 |
|---|---|---|
| `grSstWinOpen` | `cFormat` | **1 = ABGR** |
| `grSstWinOpen` | `origin` | **1 = GR_ORIGIN_LOWER_LEFT** |
| `grLfbWriteColorFormat` | `format` | **1 = ABGR** (1회 호출) |

### region 좌표는 origin 상대가 아니다 — **확인됨**

`grLfbLock`은 `GrOriginLocation_t`를 명시적으로 받지만 region 전송 두 개는 받지
않습니다. 이는 region이 프레임 버퍼를 **native 배치(행 0 = 화면 위)** 로 주소지정하기
때문입니다. 실증: `origin = LOWER_LEFT`인 이 창에서 lock과 같은 방식으로 행을
뒤집었더니 전면 화면이 **상하 반전**되어 표시되었습니다. 뒤집지 않는 것이 맞습니다.

### 8888 source word는 grLfbWriteColorFormat 순서를 따른다 — **확인됨**

`GR_LFB_SRC_FMT_*`를 "항상 명시적 RGB 순서"로 해석해 8888의 메모리 배치를
`B,G,R,A`(ARGB word)로 읽었더니 화면의 **red와 blue가 교환**되었습니다. PIU는
`grLfbWriteColorFormat(ABGR)`을 선언하므로 source word는 `A<<24|B<<16|G<<8|R`, 즉
메모리 배치가 `R,G,B,A`입니다. GrColor_t가 cFormat을 따르는 것과 같은 규칙이며,
`grLfbWriteColorFormat`이 존재하는 이유이기도 합니다. 목적지 프레임 버퍼도 ABGR이므로
두 형식이 같을 때는 무변환 통과가 되어야 합니다.

이는 design 360이 세운 "`GR_LFB_SRC_FMT_565` source는 명시적 RGB565"라는 추정을
**반증**합니다. 그 추정은 당시 검증할 수 없었습니다 — `grLfbWriteRegion`은 Task 476
전까지 한 번도 성공한 적이 없기 때문입니다. lock write mode가 cFormat을 따른다는 design
360의 결론 자체는 그대로 유효합니다.

Confirmed on 2026-08-13 from the user's `pumpit8` run log and screenshot. Both region gates fire
**1440 times each** — `3 x 480`, three full-screen 480-row passes — with `640x1` rows, `stride = 0`,
on the back buffer, `y` walking down from 479, reading into one guest buffer and writing back from
another. **Correction:** an earlier note called this "the bottom 128 rows"; 128 is
`kGlideImplementationIssueRecordCapacity`, and distinct argument sets were truncated there. Issue
record counts are not call counts — the Glide call trace `count` is.

The same log pins `grSstWinOpen` to `cFormat = 1` (ABGR) and `origin = 1` (`GR_ORIGIN_LOWER_LEFT`),
with one `grLfbWriteColorFormat(1)` call. Two consequences were confirmed by rendering:

Region coordinates are **not** origin-relative. `grLfbLock` takes an explicit
`GrOriginLocation_t` and the region entry points take none, because they address the frame buffer in
its native layout where row 0 is the top. Mirroring the row index the way a lock would rendered this
full-screen pass upside down under the lower-left window.

The 8888 source word follows `grLfbWriteColorFormat`, not a fixed RGB order. Reading its memory as
`B,G,R,A` (an ARGB word) swapped red and blue on screen; PIU declares ABGR, so the word is
`A<<24|B<<16|G<<8|R` and its bytes are `R,G,B,A`. This is the same rule `GrColor_t` follows and the
reason `grLfbWriteColorFormat` exists. It **refutes** design 360's inference that a
`GR_LFB_SRC_FMT_565` source is explicitly RGB-ordered — an inference that could not be tested at the
time, because `grLfbWriteRegion` never once succeeded before Task 476. Design 360's conclusion about
lock write modes following the cFormat still stands.
