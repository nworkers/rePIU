# 작업 지시 20260902-570 — Linux x64 segment override base+disp8

설계: [20260902-570](../design/20260902-570-linux-x64-segment-base-disp8.md)

## 구현 순서

1. `LongModeSegmentOverrideEmittable`이 기존 absolute `disp32`와 안전한 비-SIB
   `base+disp8`을 구분해 허용하도록 확장합니다.
2. long-mode access 방출에서 `disp8`을 부호 확장하고 ModRM을 `mod=10`으로 바꾸어
   patch 가능한 `disp32`를 만듭니다.
3. `original_displacement`와 `displacement_offset`이 두 형식에서 같은 patch 계약을
   따르게 합니다.
4. Linux x64 segment override probe가 absolute와 base+disp8을 함께 patch하고, 일치,
   불일치, HLE 뒤 native 복원 경로를 실행하도록 확장합니다.
5. Linux x64 Release probe/census, Linux i386 Release probe, Win32 x86 Debug probe를
   실행합니다.
6. `ARCHITECTURE.md`, Linux port frontier, 작업 로그에 결과와 다음 장벽을 반영합니다.
7. 의도한 파일만 커밋합니다.

## 완료 조건

- 실제 emitted `mov cl, [edi+disp32]`가 live ES base를 포함한 주소에서 byte를 읽습니다.
- selector 불일치 경로가 새 slot에서 access 전에 중단됩니다.
- census가 `agrees=true`이고 reachable frontier가 전진합니다.
- 지원 중인 i386/Win32 probe에 회귀가 없습니다.

---

# Work order 20260902-570 — Linux x64 segment-override base+disp8

Design: [20260902-570](../design/20260902-570-linux-x64-segment-base-disp8.md)

## Implementation order

1. Extend `LongModeSegmentOverrideEmittable` to distinguish and admit the
   existing absolute-disp32 form and the safe non-SIB base-plus-disp8 form.
2. Sign-extend disp8 during long-mode access emission, force ModRM to `mod=10`,
   and create a patchable disp32.
3. Keep `original_displacement` and `displacement_offset` under one patch
   contract for both forms.
4. Extend the Linux x64 segment-override probe to patch and execute both forms
   through match, mismatch, and HLE-to-native restoration paths.
5. Run the Linux x64 Release probe/census, Linux i386 Release probe, and Win32
   x86 Debug probe.
6. Record the result and next barrier in `ARCHITECTURE.md`, the Linux port
   frontier, and the work log.
7. Commit only the intended files.

## Completion criteria

- The actual emitted `mov cl, [edi+disp32]` reads from an address including the
  live ES base.
- Selector mismatch stops in the new slot before the access.
- The census reports `agrees=true` and advances the reachable frontier.
- Supported i386/Win32 probes have no regression.
