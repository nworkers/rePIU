# 20260730-359 Glide 배경 렌더링 정확성 작업 로그 / Work log

* 설계: [20260730-359-glide-background-rendering-accuracy.md](../design/20260730-359-glide-background-rendering-accuracy.md)
* 작업 지시: [20260730-359-glide-background-rendering-accuracy.md](../work-orders/20260730-359-glide-background-rendering-accuracy.md)

## 한국어

### 결과

원본 실행 파일과 자산을 변경하지 않고 Glide HLE의 정점, fog, LFB 경계를
보정했습니다.

* PIU의 60-byte `GrVertex`에서 dword 8 공용 `oow`와 dword 9/10 TMU0
  `sow/tow`를 분리 해독합니다. `sow/tow`만 coordinate extent로 정규화하고
  fragment shader가 보간된 공용 `oow`로 나눕니다.
* 플랫폼 공용 `glide_fog`에 공식 64-entry table knot, 구간 선택, 선형 보간,
  clamp를 구현했습니다.
* `grFogMode(0/2)`, `grFogColorValue`, `grFogTable`을 논리 상태와 GLSL
  uniform에 연결했습니다. table은 guest pointer를 보관하는 대신 호출 시
  64바이트를 복사합니다.
* LFB 표시는 geometry combine과 fog를 건너뛰는 전용 shader mode를 사용합니다.
  depth, blend, cull, alpha test, scissor, color mask, draw buffer, texture binding을
  blit 동안 격리하고 복원합니다.
* `repiu_glide_render_probe`에 fog 수학과 알려진 RGBA LFB surface의 OpenGL
  back-buffer readback 검증을 추가했습니다.

material이나 OpenGL fixed-function light는 추가하지 않았습니다. 현재 결함은
응용 프로그램이 제공하는 texture/fog 좌표 의미를 HLE가 잃은 문제였으며 조명을
재구성할 근거가 없습니다.

### 검증

다음 빌드가 성공했습니다.

```text
cmake --build build\win32_x86_debug --config Debug --target repiu_loader_win32 repiu_glide_render_probe
```

기존 파일의 C4819 코드 페이지 경고가 반복되었지만 새 컴파일 오류나 링크 오류는
없었습니다.

다음 probe가 통과했습니다.

```text
repiu_glide_render_probe=pass
repiu_glide_render_probe --opengl-lfb=pass
repiu_glide_issue_probe=pass
```

28초 `pumpit1` AOT-DBT smoke에서 다음을 확인했습니다.

* 전체 화면 texture triangle 12번 뒤 비검정 픽셀:
  `73,939/307,200`
* mode 0/2 `grFogMode` 호출 뒤 `fog-mode-backend` 및 unsupported fog 기록: 없음
* LFB staging:
  세 번째 unlock `19,224/614,400`, 이후 최대 `53,052/614,400` non-zero bytes
* staging RGBA BMP:
  `30,858` nonblack pixels, bounding box `(200,0)..(442,398)`

### LFB 판정 정정

기존 back-buffer 진단의 `non-black`은 채널값이 8보다 큰 픽셀만 셌습니다. fade
초기의 세 번째 입력은 `19,224`개 nonzero pixel이지만 최대 채널값이 4였고, 네 번째도
`23,148`개와 최대값 8이어서 기존 출력은 0이었습니다. 보정 진단에서 2배 drawable은
각각 `76,896`개/최대 4와 `92,592`개/최대 8을 보존했습니다. 픽셀 수가 정확히 4배이고
최댓값도 같으므로 실제 game-thread/host-thread LFB blit가 정상임을 확인했습니다.

일부 smoke 시도는 렌더링 진입 전 `direct control-flow target is outside the cache`
또는 relocated base 예약 실패로 종료됐습니다. 새 프로세스에서 정상 주소 배치를
얻은 실행으로 위 렌더링 증거를 수집했으며, 이 AOT 주소 배치 문제는 Task 359의
Glide 변경과 별개입니다.

---

## English

### Result

The Glide HLE vertex, fog, and LFB boundaries were corrected without changing
the original executable or assets.

* The PIU 60-byte `GrVertex` decoder now carries shared `oow` from dword 8 and
  TMU0 `sow/tow` from dwords 9/10. Only the numerators are normalized by the
  coordinate extent; the fragment shader divides them by interpolated `oow`.
* Platform-neutral `glide_fog` implements the documented 64-entry knots,
  interval selection, linear interpolation, and clamps.
* `grFogMode(0/2)`, `grFogColorValue`, and `grFogTable` now update logical
  state and GLSL uniforms. The table's 64 guest bytes are copied immediately.
* LFB presentation uses a dedicated shader bypass and temporarily isolates
  depth, blend, cull, alpha test, scissor, color mask, draw buffer, and texture
  binding.
* `repiu_glide_render_probe` covers fog math and OpenGL back-buffer readback of
  a known RGBA LFB surface.

No material or fixed-function lighting reconstruction was introduced.

### Verification

The Win32 x86 Debug loader and render probe built successfully. The new render
probe, its `--opengl-lfb` path, and the existing Glide issue probe all passed.
Only pre-existing C4819 code-page warnings remained.

A 28-second `pumpit1` AOT-DBT smoke measured 73,939/307,200 nonblack pixels
after the twelfth full-screen textured triangle and produced no fog-mode
backend or unsupported record for observed modes 0/2. The third LFB unlock
contained 19,224/614,400 nonzero staging bytes, later reaching
53,052/614,400. The decoded staging BMP contained 30,858 nonblack pixels in
bounding box `(200,0)..(442,398)`.

### Corrected LFB finding

The old back-buffer `non-black` diagnostic counted only channels above 8. The
third fade input has 19,224 nonzero pixels but maximum channel 4, and the
fourth has 23,148 at maximum 8. The corrected 2x-drawable diagnostic preserves
76,896 pixels at maximum 4 and 92,592 at maximum 8 respectively. The exact 4x
pixel count and matching maxima confirm real game-thread/host-thread LFB
presentation; the earlier zero was a threshold-interpretation error.

Some smoke attempts ended before rendering because of transient AOT cache
placement or relocated-base reservation failures. The evidence above comes
from successful fresh-process runs; that address-placement issue is separate
from Task 359's Glide changes.
