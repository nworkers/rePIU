# Paletted texture 지연 갱신 작업 지시

설계: [20260817-489-lazy-paletted-texture-refresh.md](../design/20260817-489-lazy-paletted-texture-refresh.md)

1. 기존 texture census에 palette download, 동일/변경, refresh 수·바이트·시간 계측을
   추가하고 probe로 snapshot 계약을 고정합니다.
2. Win32 OpenGL backend에 palette generation과 P_8/AP_88 entry별 적용 generation을
   추가합니다.
3. 동일 palette upload는 생략하고 실제 변경은 texture를 즉시 재업로드하지 않은 채
   dirty generation으로 기록합니다.
4. textured draw 직전에 현재 indexed texture만 디코드하고 `glTexSubImage2D`로 갱신합니다.
5. 종료 요약, `ARCHITECTURE.md`, Glide 누적 분석, KB, TODO를 현재 계약과 계측에 맞게
   갱신합니다.
6. Win32 x86 Debug/Release `repiu_aot_probe` 및 `repiu` 빌드와 전체 probe를 수행하고
   결과를 작업 로그에 기록합니다.
7. 모든 변경을 하나의 작업 커밋으로 남깁니다.

---

# Lazy Paletted-Texture Refresh Work Order

Design: [20260817-489-lazy-paletted-texture-refresh.md](../design/20260817-489-lazy-paletted-texture-refresh.md)

1. Extend the texture census with palette download, identical/change, refresh
   count/byte/time metrics and lock its snapshot contract in the probe.
2. Add a palette generation and per-P_8/AP_88 applied generations to the Win32
   OpenGL backend.
3. Suppress identical palette uploads and record real changes as dirty generations
   without immediately re-uploading textures.
4. Decode only the current indexed texture before a textured draw and update it
   through `glTexSubImage2D`.
5. Update the ending summary, `ARCHITECTURE.md`, cumulative Glide analysis, KB,
   and TODO for the resulting contract and measurements.
6. Build Win32 x86 Debug/Release `repiu_aot_probe` and `repiu`, run the complete
   probe, and record results in the work log.
7. Commit the complete task as one work unit.
