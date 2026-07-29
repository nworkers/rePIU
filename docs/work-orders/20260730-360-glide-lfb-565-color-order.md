# 20260730-360 Glide LFB 565 색 채널 순서 작업 지시 / Work Order

* 설계 / Design:
  [20260730-360-glide-lfb-565-color-order.md](../design/20260730-360-glide-lfb-565-color-order.md)

## 한국어

### 목표

`grLfbLock`의 ABGR/BGR565 버퍼를 RGB565로 오해하여 파랑이 노랑으로 바뀌는 결함을
수정합니다.

### 작업

1. LFB 565 encode/decode가 Glide color format을 받도록 확장합니다.
2. ABGR/BGRA에서 Red와 Blue의 5비트 위치를 교환합니다.
3. `grSstWinOpen`의 color format으로 LFB write format 기본값을 초기화합니다.
4. lock seeding과 unlock presentation에 유효 LFB write format을 전달합니다.
5. `grLfbWriteRegion`의 명시적 RGB565 source 의미는 유지합니다.
6. 합성 회귀 probe와 Win32 x86 빌드 및 실제 게임 smoke를 수행합니다.
7. architecture, analysis, KB와 작업 로그를 갱신하고 하나의 커밋으로 남깁니다.

### 완료 조건

* RGB565와 BGR565의 pure red/blue 및 cyan 변환 probe가 통과합니다.
* 기존 render probe와 Win32 x86 loader 빌드가 통과합니다.
* 실제 LFB 화면에서 청록/파랑이 Red와 교환되지 않습니다.
* 일반 텍스처 디코더와 OpenGL shader 출력은 변경되지 않습니다.

---

## English

### Objective and work

Fix the `grLfbLock` defect that interprets PIU's ABGR/BGR565 buffer as RGB565,
turning blue/cyan into yellow. Make the platform-neutral 565 conversion
color-format aware, initialize the LFB write format from `grSstWinOpen`, pass
the effective format through lock seeding and unlock presentation, and retain
the explicit RGB565 semantics of `grLfbWriteRegion`.

Add synthetic channel-order and round-trip coverage, build the Win32 x86
loader and render probe, run a game smoke test, update architecture, analysis,
KB, and the work log, and finish as one commit.
