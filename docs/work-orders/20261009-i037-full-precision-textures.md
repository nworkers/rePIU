# 작업 지시: 게임 내장 Mesa fx의 4444 절단을 우회하는 원본 정밀도 텍스처 (issue #37)

분석: `docs/analysis/mesa-fx-texture-path.md`
설계: `docs/design/20261009-i037-full-precision-textures.md`

1. 분석: pumpitea `PIU.EXE`에서 4444/565 절단 위치를 찾아 문서화한다. `repiu_aot_probe --dump-image`를
   더해 재배치 이미지를 뽑는다. 19개 롬셋에서 같은 Mesa 빌드인지 서명으로 확인한다.
2. `repiu/hle/mesa_fx_texture_source.h/.cpp`: 게이트 레지스터로 Mesa 객체를 따라가 원본 RGBA8을 만들고,
   링크·data·재절단 검증을 모두 통과할 때만 돌려준다. 결과는 `MesaFxSourceOutcome`.
3. 게이트(`linexe_glide_boundary`): format 12/10일 때 2를 부르고 결과를 `StoreTexture`에 넘긴다.
4. 백엔드: 항목마다 게임 데이터와 원본 정밀도 이미지를 보관하고 옵션대로 올린다. 옵션이 바뀌면 이벤트
   펌프에서 다시 올린다. census에 결과별 집계, 최종 로그 한 줄.
5. 옵션: OSD 체크박스 "Full-precision textures (8-bit)", `REPIU_GLIDE_TEXTURE_FULL_PRECISION`(기본 켬),
   런처 설정 `[Video] texture_full_precision`.
6. probe: `mesa_fx_texture_source`(core·aot), 런처 probe 갱신. CMake.
7. 문서: 분석, 설계, EXE_DESIGN, ARCHITECTURE, README.
8. 검증: Win32·Linux x64 빌드, probe, pumpitea 실행에서 used 수와 화면(사용자 환경).

완료 기준: 옵션을 켜면 검증된 텍스처가 8bit로 올라가고, 끄면 변경 전과 같으며, 검증에 실패한
텍스처는 언제나 게임 데이터를 쓴다.

---

# Work order: full-precision textures bypassing the embedded Mesa fx 4444 truncation (issue #37)

(1) Find and document where pumpitea's `PIU.EXE` cuts textures to 4444/565, adding
`repiu_aot_probe --dump-image` for the relocated image, and confirm by signature that all 19 ROM sets
share the Mesa build. (2) `repiu/hle/mesa_fx_texture_source`: follow the gate registers through the
Mesa objects to an RGBA8 original, returned only when the link, data and re-truncation checks all
pass, with a `MesaFxSourceOutcome`. (3) Call it from the gate for formats 12 and 10 and pass the
result to `StoreTexture`. (4) The backend keeps both images per entry, uploads the one the option
selects, re-uploads on the event pump after a change, and counts outcomes in the census and the final
log. (5) The option: an OSD checkbox, `REPIU_GLIDE_TEXTURE_FULL_PRECISION` (on by default) and the
launcher's `[Video] texture_full_precision`. (6) The `mesa_fx_texture_source` probe (core and aot),
the launcher probe, CMake. (7) Docs: analysis, design, EXE_DESIGN, ARCHITECTURE, README. (8) Verify:
Win32 and Linux x64 builds, probes, and a pumpitea run's used count and screen on the user's setup.
Done when the option on uploads verified textures at 8 bits, off behaves as before, and a texture that
fails a check always uses the game's data.
