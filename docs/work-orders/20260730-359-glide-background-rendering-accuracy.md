# 20260730-359 Glide 배경 렌더링 정확성 작업 지시 / Work order

* 설계: [20260730-359-glide-background-rendering-accuracy.md](../design/20260730-359-glide-background-rendering-accuracy.md)

## 한국어

### 목표

원본 실행 파일을 수정하지 않고 Glide HLE의 perspective texture, table fog,
LFB presentation 의미를 보정하여 원본의 원근 배경을 복원합니다.

### 작업

1. platform-neutral fog table index/interpolation 계산기와 합성 probe를 추가합니다.
2. `GlideDrawVertex`에 공용 reciprocal-w를 추가하고 60-byte `GrVertex`의
   dword 8 및 TMU0 dword 9/10을 해독합니다.
3. GLSL varying을 확장하여 fragment 단계에서 perspective divide를 수행합니다.
4. `grFogColorValue`, `grFogTable`, `grFogMode(0/2)`를 논리 상태와 shader
   uniform에 연결합니다.
5. LFB 전용 shader bypass와 alpha/scissor/color-mask/draw-buffer 상태 저장·복원을
   구현합니다.
6. 관련 architecture, analysis, KB를 갱신하고 런타임 증거를 작업 로그에
   기록합니다.

### 완료 조건

* fog 계산 probe와 기존 probe가 모두 통과합니다.
* Win32 x86 빌드가 성공합니다.
* 런타임에서 mode 2 fog backend failure가 0이고 LFB nonblack 검증이 통과합니다.
* 원본 캡처와 비교하여 체크무늬 배경의 perspective mapping이 복원됩니다.
* 변경을 하나의 작업 단위 커밋으로 남깁니다.

---

## English

### Objective and work

Correct perspective texture mapping, table fog, and LFB presentation in the
Glide HLE without modifying the original executable.

Add a platform-neutral fog lookup/interpolation helper and probe; decode shared
reciprocal-w from dword 8 and TMU0 `sow/tow` from dwords 9/10; divide them in
the fragment shader; connect fog color, copied table, and observed modes 0/2
to shader state; isolate LFB blits from all relevant geometry state; and update
architecture, analysis, KB, and the work log.

### Completion

The new and existing probes must pass, the Win32 x86 build must succeed, the
runtime must report no mode-2 fog backend failures and retain nonblack LFB
pixels, the original perspective checkerboard composition must be restored,
and the task must end as one Git commit.
