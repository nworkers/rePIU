# Glide texture-table guest stack ABI 정정 작업 로그

설계: [20260814-483-glide-texture-table-stack-abi.md](../design/20260814-483-glide-texture-table-stack-abi.md)
작업 지시: [20260814-483-glide-texture-table-stack-abi.md](../work-orders/20260814-483-glide-texture-table-stack-abi.md)

## 결과

- `_GRTEXDOWNLOADTABLE@12`의 guest frame을 반환 주소, TMU, type, data의 네 dword로
  해석하는 `DecodeGlideTexDownloadTableCall`을 추가했습니다.
- handler가 palette type을 index 2, data pointer를 index 3에서 읽도록 정정했습니다.
- 성공·unsupported·읽기 실패를 포함한 handled path의 stack advance를 16바이트로
  정정했습니다.
- 원 로그 frame `0x04012345, 0, 2, 0x04ACA200`과 짧은/null 입력을 검사하는
  `glide_texture_table_stack_probe`를 전체 AOT probe에 연결했습니다.
- 사용자 재실행에서 나타난 palette 색상 손상을 추적해 표준 palette 상위 8비트가
  alpha가 아니라 무시되는 값임을 확인했습니다. palette alpha를 255로 정규화하고,
  P_8의 불투명 RGB 및 AP_88의 texel alpha 의미를 같은 probe에 추가했습니다.
- alpha 정규화 뒤에도 화면이 동일하다는 재확인과 원 로그 호출 순서로, texture가
  palette보다 먼저 올라오는 수명 결함을 확인했습니다. backend가 P_8/AP_88 원본을
  보존하고 palette download마다 기존 indexed texture만 갱신하도록 수정했습니다.

## 검증

- Win32 x86 Debug `repiu_aot_probe`, `repiu`: 빌드 성공. 기존 C4819와 LNK4217
  경고만 남았습니다.
- Debug 전체 AOT probe: exit 0,
  `glide_texture_table_stack_probe=pass`.
- Win32 x86 Release `repiu_aot_probe`, `repiu`: 328초 전체 재컴파일, exit 0.
- Release 전체 AOT probe: exit 0,
  `glide_texture_table_stack_probe=pass` 및 모든 `*_all=true`.
- palette 보강 후 Debug/Release 증분 빌드와 probe도 exit 0입니다. probe는
  `0x12ABCDEF` entry를 `AB CD EF FF`로 변환하고, P_8 alpha `FF`와 AP_88 texel
  alpha `40`을 각각 확인합니다.
- indexed texture 수명주기 보강 뒤에도 Win32 x86 Debug/Release의
  `repiu_aot_probe`, `repiu` 빌드가 성공했습니다. 두 구성의 전체 probe에서
  `glide_texture_table_stack_probe=pass`와 `glide_texture_census_all=true`를
  다시 확인했습니다.
- Release 12초/15초 제한 실행: 정상 timeout exit 3, Glide implementation issue
  `unimplemented/unsupported/backend/abi/unique/overflow = 0/0/0/0/0/0`.
  다만 무입력 장면은 call-audit에서 `_GRTEXDOWNLOADTABLE@12`에 도달하지 않았습니다.

## 사용자 실기 확인과 후속 항목

사용자가 기존 `pumpipx3` 장면에서 palette 색상이 정상으로 복구됐음을 확인했습니다.
동시에 성능이 크게 저하됐습니다. 현재 palette download마다 모든 보존 P_8/AP_88 texture를
CPU 재디코드하고 `glTexImage2D`로 다시 올리는 정확성 우선 경로가 원인 후보입니다. 이번
릴리스에서는 동작을 유지하며, 계측과 최적화 구현은 `docs/TODO.md`의 후속 작업으로 넘깁니다.

---

# Glide Texture-Table Guest Stack ABI Correction Work Log

Design: [20260814-483-glide-texture-table-stack-abi.md](../design/20260814-483-glide-texture-table-stack-abi.md)
Work order: [20260814-483-glide-texture-table-stack-abi.md](../work-orders/20260814-483-glide-texture-table-stack-abi.md)

## Result

Added `DecodeGlideTexDownloadTableCall` for the four-dword return/TMU/type/data
frame, corrected palette type and data to indices 2 and 3, and corrected every
handled path to advance the stack by 16 bytes. Added the exact logged frame plus
short/null cases to the full AOT probe. After the user exposed corrupted palette
colour, confirmed that the standard palette high byte is ignored, normalized
palette alpha to 255, and added P_8/AP_88 alpha semantics to the same probe.
When the user confirmed that this alone did not change the screen, the original
call ordering exposed the lifetime bug: texture upload precedes palette upload.
The backend now retains indexed sources and refreshes existing P_8/AP_88
textures on every palette download.

## Verification

Win32 x86 Debug and Release builds of `repiu_aot_probe` and `repiu` succeeded
with only the existing C4819 and LNK4217 warnings. Both full probes exited zero
with `glide_texture_table_stack_probe=pass`; Release also reported every
`*_all=true`. Twelve- and fifteen-second Release timeout runs ended normally
with all Glide implementation issue totals zero, but the unattended scene did
not reach `_GRTEXDOWNLOADTABLE@12` in call-audit. After the palette correction,
both incremental builds and probes also exited zero; the probe maps
`0x12ABCDEF` to `AB CD EF FF`, gives P_8 alpha `FF`, and retains AP_88 texel
alpha `40`. After the indexed-texture lifetime correction, the Win32 x86 Debug
and Release builds of both targets succeeded again. Both full probes reported
`glide_texture_table_stack_probe=pass` and `glide_texture_census_all=true`.

## User runtime confirmation and follow-up

The user confirmed that palette colours are restored in the original `pumpipx3`
scene and also reported a severe performance loss. The accuracy-first path that
CPU-decodes and re-uploads every retained P_8/AP_88 texture through `glTexImage2D`
after every palette download is the current candidate cause. This release keeps
the correct behaviour and defers measurement and optimization to `docs/TODO.md`.
