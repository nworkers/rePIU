# 작업 지시: Glide 텍스처 census와 픽셀 덤프 / Work order: Glide texture census and pixel dump

Task 375. 설계: [20260801-375](../design/20260801-375-glide-texture-census-and-dump.md)

## 한국어

### 목표

텍스처 업로드의 속성을 항상 집계하고, 필요할 때 실제 픽셀을 덤프해 눈으로 확인할
수 있게 한다. 텍스처가 범인인지 아닌지를 가르는 것이 목적이다.

### 단계

1. **census 모듈 추가**
   * `include/repiu/platform/win32/glide_texture_census.h`
   * `src/platform/win32/glide_texture_census.cpp`
   * `Win32GlideTextureCensus` — upload 수, 고유 주소 수, **동일 내용 재업로드 수**,
     내용 변경 재업로드 수, 포맷 히스토그램, 크기 히스토그램, 디코드 실패 수와
     마지막 실패 포맷, **extent≠픽셀 크기 건수**, 총 디코드 바이트.
   * `RecordGlideTextureUpload(...)` — 주소별 마지막 내용 해시를 보관해 비교.
   * `ResolveGlideTextureDumpSetting(...)` / `GlideTextureDumpDirectory()`.
   * snapshot 구조체.
   * `CMakeLists.txt` 등록.

2. **TGA 덤프**
   * 32비트 무압축, origin 상단(Glide 좌표계와 일치).
   * 파일명 `tex_<seq>_<addr>_<w>x<h>_fmt<F>_lod<L>_ar<A>.tga`.
   * `manifest.csv` — 헤더 1줄 + 업로드당 1줄(파일명, 순번, 주소, 포맷, large_lod,
     aspect_ratio, w, h, s_extent, t_extent, source_size, palette, hash).
   * **상한 512건**(`REPIU_GLIDE_TEX_DUMP_LIMIT`로 조정).
   * 기본 **off**. 값이 경로면 그 디렉터리, `1`이면 `build/texture_dumps/`.

3. **backend 연결** (`glide_opengl_backend.cpp` `StoreTexture`)
   * 디코드 성공 직후 census 기록, 필요 시 덤프.
   * **디코드 실패도 census에 기록**해야 조용히 사라지는 텍스처가 보인다.
   * 기존 `REPIU_GLIDE_TEX_DIAG` 동작은 유지.

4. **요약 출력**
   * `Win32 Glide texture census uploads/distinct/identical-repeats/changed-repeats`
   * `Win32 Glide texture census decode-failures/last-failed-format/extent-mismatch`
   * `Win32 Glide texture census bytes/dump-written/dump-limited`
   * 포맷·크기 히스토그램 상위 항목.

5. **probe 추가**
   * `glide_texture_census_probe.{h,cpp}` + `main.cpp` + `CMakeLists.txt`.
   * 검증: 동일 내용 재업로드가 identical로 분류, 내용 변경이 changed로 분류,
     새 주소가 distinct 증가, 디코드 실패 기록, extent 불일치 감지,
     덤프 설정 파싱, `nullptr` 무해.

6. **빌드·측정**
   * Debug + Release, probe exit 0.
   * music select를 `REPIU_GLIDE_TEX_DUMP=1`로 캡처하고 덤프를 육안 확인.
   * **덤프를 켠 실행의 타이밍은 성능 판정에 쓰지 않는다.**

7. **문서**
   * 작업 로그, `docs/analysis/glide2x-ovl-and-opengl-hle.md` 갱신.

### 완료 조건

* census가 요약에 출력되고 동일/변경 재업로드가 구분됨.
* `REPIU_GLIDE_TEX_DUMP=1`로 TGA와 manifest가 생성되고 뷰어로 열림.
* 기본 실행에서 덤프 파일이 생성되지 않음.
* probe 통과, 양 구성 빌드 성공.

### 비범위

* 업로드 캐시(내용 해시 기반 생략) 구현은 하지 않는다. census 결과를 보고 별건으로
  판단한다.
* 예외 축(Task 373)과 Glide gate 재평가는 별건.

---

## English

Make texture uploads inspectable at both the attribute and pixel level so the
suspicion about textures can be settled. Add an always-on census recording uploads
against distinct addresses, repeats whose content hash is unchanged versus changed,
format and dimension histograms, decode failures with the last failing format, how
often the s/t extent differs from the pixel size, and total decoded bytes — decode
failures included, since a silently dropped texture is exactly what would not
otherwise show. Add an opt-in pixel dump behind `REPIU_GLIDE_TEX_DUMP` writing
uncompressed 32-bit top-origin TGA files plus a `manifest.csv` row per upload,
capped at 512. Wire both into `StoreTexture` without disturbing the existing
`REPIU_GLIDE_TEX_DIAG`, print the census in the exit summary, and cover the
classification logic with a probe. Verify with both build configurations, a passing
probe, a music-select capture with the dump on for visual inspection, and the
absence of dump files in a default run. Timing from a dump-enabled run is not used
for performance judgement. Implementing an upload cache is out of scope until the
census says whether one is warranted.
