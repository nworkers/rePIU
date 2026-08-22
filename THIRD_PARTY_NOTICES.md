# Third-Party Notices

## Zydis v4.1.1

rePIU vendors the official amalgamated source generated from Zydis tag `v4.1.1` (`a2278f1d254e492f6a6b39f6cb5d1f5d515659dc`) and its pinned Zycore submodule (`0b2432ced0884fd152b471d97ecf0258ff4d859f`). Both projects use the MIT License.

- Project: https://github.com/zyantific/zydis
- Zydis license: [`third_party/zydis/LICENSE-ZYDIS`](third_party/zydis/LICENSE-ZYDIS)
- Zycore license: [`third_party/zydis/LICENSE-ZYCORE`](third_party/zydis/LICENSE-ZYCORE)

Generated file SHA-256 values:

- `Zydis.c`: `1851E6D2EC6A681915D3723A96516A387F628ECA6E092A04BEDE78198CDF644A`
- `Zydis.h`: `CE154FC859C134C1DF5BC62A9DFE1C427135812C1A7BC1C54BA180EA27A70E55`

The vendored amalgamation is used only for x86 instruction decoding and metadata. Original PIU code continues to execute directly; Zydis does not replace game or resource-processing logic.

## Dear ImGui v1.92.1
rePIU fetches Dear ImGui tag `v1.92.1` and builds only its core sources with the
`imgui_impl_sdl3` and `imgui_impl_opengl3` backends, which draw the launcher inside the SDL3
window the host already creates. Dear ImGui uses the MIT License.
- Upstream: https://github.com/ocornut/imgui
- License text ships with the fetched sources as `LICENSE.txt`.

## libchdr v0.3.0

rePIU vendors libchdr tag `v0.3.0` (commit `93d8c239ff0d4e8d7722985992649fce12d2463b`) to read MAME CHDv1-v5 images. rePIU uses its read-only CHD hunk API and implements the pumpit1 ISO9660 mount separately.

- Project: https://github.com/rtissera/libchdr
- License: [`third_party/libchdr/LICENSE.txt`](third_party/libchdr/LICENSE.txt) (BSD 3-Clause)
- Bundled LZMA SDK is public domain as recorded in [`deps/lzma-25.01/LICENSE`](third_party/libchdr/deps/lzma-25.01/LICENSE).
- Bundled miniz uses the MIT license stated at the top of [`miniz.c`](third_party/libchdr/deps/miniz-3.1.1/miniz.c).
- Bundled zstd decompressor is dual-licensed; rePIU selects its BSD license option as stated in [`zstddeclib.c`](third_party/libchdr/deps/zstd-1.5.7/zstddeclib.c). No GPL option is used.

libchdr decodes storage assets only. It does not provide CPU, machine, game-logic, or full-system emulation.

## MAME PIU10 and CAT702 algorithms

### 한국어

rePIU의 플랫폼 공용 PIU10 ISA register와 CAT702 PIU 직렬 상태 모델은 MAME의
`xtom3d_piu10.cpp`(copyright windyfairy)와 `cat702.cpp`(copyright smf)를 참고하여
이식했습니다. 두 파일은 BSD 3-Clause License입니다. MAME emulator나 CPU/machine
framework는 통합하지 않았습니다.

- Project: https://github.com/mamedev/mame
- PIU10 source: https://github.com/mamedev/mame/blob/master/src/mame/misc/xtom3d_piu10.cpp
- CAT702 source: https://github.com/mamedev/mame/blob/master/src/devices/machine/cat702.cpp

### English

rePIU's platform-neutral PIU10 ISA registers and CAT702 PIU serial state model are adapted from
MAME's `xtom3d_piu10.cpp` (copyright windyfairy) and `cat702.cpp` (copyright smf), both under the
BSD 3-Clause License. No MAME emulator, CPU, or machine framework is integrated.

Copyright (c) MAME contributors identified above. All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted
provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of
   conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice, this list of
   conditions and the following disclaimer in the documentation and/or other materials provided
   with the distribution.
3. Neither the name of the copyright holder nor the names of its contributors may be used to
   endorse or promote products derived from this software without specific prior written
   permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

## minimp3

### 한국어

rePIU는 PIU10 MAS3507D MP3 stream의 frame decode를 위해 upstream `minimp3` commit
`ea99364f61c14656440e8d77e9c233ccf3124633`을 FetchContent로 고정합니다. `minimp3`는
CC0 1.0 Universal로 제공됩니다. rePIU는 MAME의 MAS3507D 또는 MP3 wrapper 코드를
포함하지 않습니다.

- minimp3: https://github.com/lieff/minimp3
- 고정 commit: https://github.com/lieff/minimp3/commit/ea99364f61c14656440e8d77e9c233ccf3124633
- CC0 1.0: https://github.com/lieff/minimp3/blob/ea99364f61c14656440e8d77e9c233ccf3124633/LICENSE

### English

rePIU pins upstream `minimp3` commit
`ea99364f61c14656440e8d77e9c233ccf3124633` through FetchContent for frame decoding of the
PIU10 MAS3507D MP3 stream. `minimp3` is offered under CC0 1.0 Universal. rePIU does not
incorporate MAME's MAS3507D or MP3 wrapper code.
