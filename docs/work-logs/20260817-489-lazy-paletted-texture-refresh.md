# Paletted texture 지연 갱신 작업 로그

설계: [20260817-489-lazy-paletted-texture-refresh.md](../design/20260817-489-lazy-paletted-texture-refresh.md)
작업 지시: [20260817-489-lazy-paletted-texture-refresh.md](../work-orders/20260817-489-lazy-paletted-texture-refresh.md)

## 결과

- Task 483 후속 TODO를 이번 작업에 포함했습니다.
- Win32 OpenGL backend가 마지막 palette byte와 generation을 보존하도록 변경했습니다.
- 동일 palette download는 texture 작업과 generation 증가를 모두 생략합니다.
- 실제 palette 변경도 보존된 indexed texture 전부를 즉시 순회하지 않고 generation만
  증가시킵니다.
- P_8/AP_88 texture entry는 마지막 적용 generation을 기록합니다. textured draw 직전에
  현재 texture가 오래됐을 때만 보존 source를 RGBA로 디코드하고 기존 allocation을
  `glTexSubImage2D`로 갱신합니다.
- draw 공용 준비 경로에서 decode/upload 실패를 반환하므로 실패한 texture를 갱신 완료로
  오인하지 않습니다.
- texture census와 종료 로그에 palette download/changed/identical,
  lazy refresh/failure, source/RGBA byte, decode/upload nanosecond 누계를 추가했습니다.
- `docs/TODO.md`의 Task 483 palette 성능 후속 항목을 제거하고 아키텍처, 누적 Glide 분석,
  texture KB를 새 계약으로 갱신했습니다.

## 검증

- Win32 x86 Debug `repiu_aot_probe`, `repiu`: 빌드 성공. 전체 헤더 영향으로 초기 빌드가
  길어졌지만 최종 exit 0이며 기존 C4819/LNK4217 경고만 남았습니다.
- Debug `repiu_aot_probe build/runtime_mounts/pumpitp3/PIU/PIU.EXE`: exit 0,
  `glide_texture_table_stack_probe=pass`,
  `glide_texture_census_palette_refresh_profiled=true`,
  `glide_texture_census_all=true`를 포함해 전체 probe 통과.
- Win32 x86 Release `repiu_aot_probe`, `repiu`: 193초 빌드, exit 0. 기존 경고만 남았습니다.
- Release의 같은 `pumpitp3` 전체 probe: exit 0, `valid=true`, `cache_valid=true` 및 위 세
  Glide probe 항목 통과.
- 실제 `pumpitp3` 타이틀 화면의 색상과 FPS 개선 폭은 interactive 사용자 재실행으로
  남겨 두었으며, 사용자가 같은 화면에서 성능 개선을 확인했습니다. 종료 로그의
  `Win32 Glide palette ...` 세 줄은 실제 palette 변경 빈도와 남은 refresh 비용을
  계속 제공합니다.

---

# Lazy Paletted-Texture Refresh Work Log

Design: [20260817-489-lazy-paletted-texture-refresh.md](../design/20260817-489-lazy-paletted-texture-refresh.md)
Work order: [20260817-489-lazy-paletted-texture-refresh.md](../work-orders/20260817-489-lazy-paletted-texture-refresh.md)

## Result

- Folded the Task 483 follow-up TODO into this task.
- The Win32 OpenGL backend now retains the last palette bytes and a generation.
- A byte-identical palette download skips both texture work and generation advance.
- A real change advances only the generation instead of immediately walking every
  retained indexed texture.
- Each P_8/AP_88 entry records its applied generation. Immediately before a textured
  draw, only a stale current texture is decoded from its retained source and updated
  in the existing allocation with `glTexSubImage2D`.
- Decode/upload failures propagate from shared draw preparation and never mark a
  failed generation as applied.
- Texture census and ending logs now include palette downloads/changes/identicals,
  lazy refreshes/failures, source/RGBA bytes, and decode/upload nanosecond totals.
- Removed the completed Task 483 performance follow-up from `docs/TODO.md` and updated
  architecture, cumulative Glide analysis, and the texture KB.

## Verification

- Win32 x86 Debug `repiu_aot_probe` and `repiu` built successfully. The initial
  header-driven rebuild was long but ultimately exited zero with only existing
  C4819/LNK4217 warnings.
- Debug `repiu_aot_probe build/runtime_mounts/pumpitp3/PIU/PIU.EXE` exited zero;
  `glide_texture_table_stack_probe=pass`,
  `glide_texture_census_palette_refresh_profiled=true`, and
  `glide_texture_census_all=true`, with every complete-probe check passing.
- Win32 x86 Release `repiu_aot_probe` and `repiu` built in 193 seconds and exited
  zero with only existing warnings.
- The same Release `pumpitp3` full probe exited zero with `valid=true`,
  `cache_valid=true`, and the three Glide checks above.
- The user reran the same `pumpitp3` title screen and confirmed the performance
  improvement. The three ending `Win32 Glide palette ...` lines continue to expose
  the real palette-change frequency and remaining refresh cost.
