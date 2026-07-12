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

## libchdr v0.3.0

rePIU vendors libchdr tag `v0.3.0` (commit `93d8c239ff0d4e8d7722985992649fce12d2463b`) to read MAME CHDv1-v5 images. rePIU uses its read-only CHD hunk API and implements the pumpit1 ISO9660 mount separately.

- Project: https://github.com/rtissera/libchdr
- License: [`third_party/libchdr/LICENSE.txt`](third_party/libchdr/LICENSE.txt) (BSD 3-Clause)
- Bundled LZMA SDK is public domain as recorded in [`deps/lzma-25.01/LICENSE`](third_party/libchdr/deps/lzma-25.01/LICENSE).
- Bundled miniz uses the MIT license stated at the top of [`miniz.c`](third_party/libchdr/deps/miniz-3.1.1/miniz.c).
- Bundled zstd decompressor is dual-licensed; rePIU selects its BSD license option as stated in [`zstddeclib.c`](third_party/libchdr/deps/zstd-1.5.7/zstddeclib.c). No GPL option is used.

libchdr decodes storage assets only. It does not provide CPU, machine, game-logic, or full-system emulation.
