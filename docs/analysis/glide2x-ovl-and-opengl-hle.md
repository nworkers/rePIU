# Glide2x.ovl과 OpenGL HLE 분석

## 확인된 바이너리 구조

**확인됨.** 사용자 asset `PIU/Glide2x.ovl`은 MZ 뒤에 LE image를 가진 80386 OS/2 module입니다. 현재 LE parser로 3개 object, 78개 page, 3,419개 internal fixup을 해석할 수 있습니다. import module은 없습니다. 그러나 이 코드를 실행하면 원본 3Dfx 하드웨어·PCI·port I/O 경로까지 함께 요구하므로 host OpenGL 연동에는 module 전체 실행보다 export facade가 적합합니다.

resident-name table에는 module 이름 `glide2x`와 ordinal 1~172가 들어 있습니다. 주요 예는 다음과 같습니다.

| ordinal | decorated export | 확인 가능한 ABI |
|---:|---|---|
| 32 | `_GRGLIDEINIT@0` | 인자 0바이트 |
| 38 | `_GRSSTSELECT@4` | 인자 4바이트 |
| 73 | `_GRDRAWTRIANGLE@12` | 인자 12바이트 |
| 85 | `_GRBUFFERSWAP@4` | 인자 4바이트 |
| 112 | `_GRLFBLOCK@24` | 인자 24바이트 |
| 118 | `_GRSSTWINOPEN@28` | 인자 28바이트 |
| 138 | `_GRTEXSOURCE@16` | 인자 16바이트 |
| 144 | `_GRTEXDOWNLOADMIPMAPLEVELPARTIAL@40` | 인자 40바이트 |

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

# Glide2x.ovl and OpenGL HLE Analysis

The user-provided `Glide2x.ovl` is an MZ-bound 80386 LE module with three objects, 78 pages, 3,419 internal fixups, and no imported modules. Its resident-name table exposes 172 decorated exports. Suffixes such as `_GRSSTWINOPEN@28`, `_GRDRAWTRIANGLE@12`, and `_GRBUFFERSWAP@4` provide the callee argument-byte count needed for dynamic guest trap gates.

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

 # # #   ` g r T e x M a x A d d r e s s `   �  ` g r T e x M i n A d d r e s s `   ����  A B I   P��
 \��  P I U   ����  ��  �|�X�  ���  ����  ` _ G R T E X M A X A D D R E S S @ 4 `   �  ` _ G R T E X M I N A D D R E S S @ 4 `   8֜�  ������  �x�  ��( ` 0 x C 0 0 0 0 0 0 5 ` )  �   �0���ŵ�Ȳ�.   
 ��x�@�  G l i d e   2 . x   A P I  �  t�  h���D�  ` s t d c a l l `   �|���0�  4 �tǸ�  Ȭ����<�\�  �X�X��  ��<ǘ�,   ����  T�ܴ��  ` c d e c l `   )���  9�@�  ��ɤ�0�  �|���0�  8֜�  ܭ}�D�  ����X���  ������  xǐ�|�   ���#���  J��( P U S H   X���  J�L�)   8֜�X�0�  L�8���Ȳ�.   
 H L E   �̬������  ܭ}���  ޹��  ` E S P   + =   8 ` ( �X�  �Ȍ�  4   +   xǐ�  4 ) D�  Ȭ�X�X�  ����D�  ` E S P   + =   4 ` ( �X�  �Ȍ�  4 ) ̹  Ȭ�X�ĳ]�  ��h�<�\�h�,   
 �L�  H L E   �ƭ�  ��( ` _ G R C L I P W I N D O W @ 1 6 ` )   ���XՔ�  xǐ�  $��,�( m i s a l i g n m e n t ) ��  �ǻ��  R e t u r n   P o i n t   \�X�  ��0�|�  )��Ɉյ�Ȳ�.  
 