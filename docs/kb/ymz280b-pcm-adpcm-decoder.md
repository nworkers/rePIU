# YMZ280B PCM/ADPCM 디코더 / YMZ280B PCM/ADPCM decoder

## 한국어

### 1. 개요

Yamaha YMZ280B는 8보이스 PCM/ADPCM 재생 전용 칩입니다. 외부 메모리(최대 16 MB
ROM 또는 SRAM)에 저장된 음원을 읽어 스테레오 16비트 2의 보수 MSB-first 형식으로
출력합니다. 아케이드 사운드 보드에서 널리 쓰였고, 안다미로 PIU10 ISA 보드도 이 칩을
씁니다.

데이터시트가 명시하는 사양은 다음과 같습니다.

| 항목 | 사양 |
| :--- | :--- |
| 보이스 수 | 8 |
| 음원 형식 | 4-bit ADPCM, 8-bit PCM, 16-bit PCM |
| 외부 메모리 | 최대 16 MB (8비트 폭, access time 150 ns 이하) |
| 재생 주파수 | ADPCM 0.172~44.1 kHz (256단계), PCM 0.172~88.2 kHz (512단계) |
| 볼륨 | 256단계 total level |
| 팬 | 16단계 panpot |
| 출력 | 스테레오 16비트 |

출처: MAME [src/devices/sound/ymz280b.cpp](https://github.com/mamedev/mame/blob/master/src/devices/sound/ymz280b.cpp)
헤더 주석이 인용한 LSI-4MZ280B3 데이터시트.

### 2. 호스트 인터페이스

호스트에서 보이는 레지스터는 두 개뿐입니다.

| 오프셋 | 쓰기 | 읽기 |
| :--- | :--- | :--- |
| 0 | 접근할 내부 레지스터 번호 선택 | 외부 메모리 readback 래치 |
| 1 | 선택된 레지스터에 데이터 기록 | 상태 레지스터(읽으면 클리어) |

따라서 레지스터 하나를 쓰려면 항상 오프셋 0에 번호, 오프셋 1에 값을 연속으로 씁니다.

### 3. 내부 레지스터 맵

하위 `0x00`~`0x7F`는 보이스별 반복 구조입니다. 보이스 번호는 `(reg >> 2) & 7`이고
기능은 `reg & 0xE3`로 결정됩니다.

| `reg & 0xE3` | 기능 |
| :--- | :--- |
| `0x00` | 피치(fnum) 하위 8비트 |
| `0x01` | 피치 상위 1비트(d0), 루프(d4), 모드(d5-6), key on(d7) |
| `0x02` | total level (0~255) |
| `0x03` | panpot (0~15, 8이 중앙) |
| `0x20`/`0x40`/`0x60` | start 주소 상위/중위/하위 |
| `0x21`/`0x41`/`0x61` | loop start 주소 상위/중위/하위 |
| `0x22`/`0x42`/`0x62` | loop end 주소 상위/중위/하위 |
| `0x23`/`0x43`/`0x63` | stop 주소 상위/중위/하위 |

모드 필드(d5-6)는 `1`=ADPCM, `2`=PCM8, `3`=PCM16이며 `0`은 존재하지 않아 key-off와
같이 처리됩니다.

상위 레지스터는 개별 기능입니다.

| 레지스터 | 기능 |
| :--- | :--- |
| `0x80`~`0x82` | DSP 관련 (MAME도 미구현) |
| `0x84`/`0x85`/`0x86` | 외부 메모리 readback 주소 상위/중위/하위 |
| `0x87` | 외부 RAM 쓰기 |
| `0xFE` | IRQ 마스크 |
| `0xFF` | d7 key-on enable, d6 외부 메모리 enable, d4 IRQ enable |

`0xFF`의 key-on enable은 마스터 스위치입니다. 이 비트가 0이면 어떤 보이스도 key-on
되지 않고, 1→0 전이는 전 보이스를 정지시키며, 0→1 전이는 key-on 상태이면서 루프
설정된 보이스를 다시 재생시킵니다.

### 4. 주소 단위 — 가장 흔한 오해

**주소 레지스터가 담는 값은 바이트 주소가 아닙니다.** 상위/중위/하위 바이트가 각각
`<<17`, `<<9`, `<<1`로 합성되므로, 결과는 바이트 주소를 1비트 왼쪽으로 시프트한
"니블 단위" 값입니다.

```
address_in_nibbles = (high << 17) | (mid << 9) | (low << 1)
byte_address       = address_in_nibbles / 2
```

재생 위치도 같은 니블 단위로 유지되며, 모드마다 전진량만 다릅니다.

| 모드 | 샘플당 position 증가 | ROM 접근 |
| :--- | :--- | :--- |
| ADPCM | 1 (니블 하나) | `read_byte(position / 2)`의 상위 또는 하위 니블 |
| PCM8 | 2 (바이트 하나) | `read_byte(position / 2)` |
| PCM16 | 4 (바이트 둘) | `read_byte(position / 2)`와 `+1` |

ADPCM에서 상위 니블이 먼저이며, `(~position & 1) << 2`만큼 시프트해 고릅니다.

이 단일 규칙 하나가 세 모드를 모두 덮기 때문에, 모드별로 주소를 따로 변환하려 들면
반드시 어긋납니다.

### 5. 재생 주파수

```
source_rate = output_rate * output_step / 512
output_step = (ADPCM ? fnum & 0xFF : fnum & 0x1FF) + 1
output_rate = master_clock / 384 * 2
```

`16.9344 MHz` 클럭이면 `output_rate`는 `88200 Hz`입니다. ADPCM은 `output_step`이
최대 256이므로 상한이 44.1 kHz, PCM은 최대 512이므로 88.2 kHz가 되어 데이터시트
사양과 정확히 맞습니다.

### 6. ADPCM 디코드

예측기 하나와 스텝 크기 하나를 유지하는 단순 구조입니다.

```
signal += (step * diff_lookup[nibble]) / 8         // -32768..32767로 클램프
step    = (step * index_scale[nibble & 7]) >> 8    // 0x7F..0x6000으로 클램프
```

* `diff_lookup[n] = (n & 8 ? -1 : 1) * (2 * (n & 7) + 1)`
* `index_scale = {0x0E6, 0x0E6, 0x0E6, 0x0E6, 0x133, 0x199, 0x200, 0x266}`
* key-on 시 `signal = 0`, `step = 0x7F`

**루프 재생 시 주의**: 예측기는 이력 의존적이므로, loop start 지점에 처음 도달했을
때의 `signal`/`step`을 저장해 두었다가 wrap마다 복원해야 합니다. 예측기를 초기화하면
루프 경계에서 클릭 잡음이 납니다.

### 7. 볼륨과 팬

```
pan == 8 : left = right = level
pan <  8 : left = level,                   right = (pan == 0) ? 0 : level * (pan - 1) / 7
pan >  8 : left = level * (15 - pan) / 7,  right = level
```

믹싱은 보이스마다 `보간샘플 * 채널볼륨 / 2`를 누산하고 `32768 * 256`으로 정규화합니다.
즉 s16 출력은 누산값을 256으로 나눈 뒤 클램프한 값입니다.

### 8. 상태 레지스터와 IRQ

보이스가 stop 주소에 도달하면 해당 비트가 상태 레지스터에 설정됩니다. IRQ는
`0xFE` 마스크와 `0xFF`의 IRQ enable을 모두 통과해야 발생합니다. 상태 레지스터는
읽는 순간 클리어됩니다. IRQ 라인이 배선되지 않은 보드에서는 상태 레지스터만
의미가 있습니다.

### 9. rePIU에서의 구현 위치

* 칩 코어: [src/sound/ymz280b_device.cpp](../../src/sound/ymz280b_device.cpp)
* PIU10 보드 배선과 포트 디코드: [docs/analysis/piu-io-port-specification.md](../analysis/piu-io-port-specification.md)

---

## English

### 1. Overview

The Yamaha YMZ280B is a playback-only 8-voice PCM/ADPCM chip. It reads sample data
from external memory (up to 16 MB of ROM or SRAM) and outputs stereo 16-bit
two's-complement MSB-first audio. It was widely used on arcade sound boards,
including Andamiro's PIU10 ISA board.

| Item | Specification |
| :--- | :--- |
| Voices | 8 |
| Sample formats | 4-bit ADPCM, 8-bit PCM, 16-bit PCM |
| External memory | up to 16 MB (8 bits wide, 150 ns access or faster) |
| Playback frequency | ADPCM 0.172–44.1 kHz in 256 steps; PCM 0.172–88.2 kHz in 512 steps |
| Volume | 256-step total level |
| Pan | 16-step panpot |
| Output | stereo 16-bit |

Source: the LSI-4MZ280B3 datasheet as quoted in the MAME
[src/devices/sound/ymz280b.cpp](https://github.com/mamedev/mame/blob/master/src/devices/sound/ymz280b.cpp)
header comment.

### 2. Host interface

The host sees only two registers: offset 0 selects the internal register number on
write and returns the external memory readback latch on read; offset 1 writes data
to the selected register and returns the status register (cleared by reading) on
read. Programming any register is therefore always a number write followed by a
data write.

### 3. Internal register map

Registers `0x00`–`0x7F` repeat per voice. The voice is `(reg >> 2) & 7` and the
function is `reg & 0xE3`:

| `reg & 0xE3` | Function |
| :--- | :--- |
| `0x00` | pitch (fnum) low 8 bits |
| `0x01` | pitch high bit (d0), loop (d4), mode (d5-6), key on (d7) |
| `0x02` | total level (0–255) |
| `0x03` | panpot (0–15, 8 is centre) |
| `0x20`/`0x40`/`0x60` | start address high/mid/low |
| `0x21`/`0x41`/`0x61` | loop start address high/mid/low |
| `0x22`/`0x42`/`0x62` | loop end address high/mid/low |
| `0x23`/`0x43`/`0x63` | stop address high/mid/low |

The mode field is `1` = ADPCM, `2` = PCM8, `3` = PCM16; mode `0` does not exist and
behaves as key-off.

Upper registers are individual: `0x80`–`0x82` are DSP related (unimplemented in
MAME too), `0x84`/`0x85`/`0x86` form the external memory readback address,
`0x87` writes external RAM, `0xFE` is the IRQ mask, and `0xFF` carries key-on
enable (d7), external memory enable (d6), and IRQ enable (d4).

Key-on enable in `0xFF` is a master switch: no voice keys on while it is clear, a
1→0 transition stops every voice, and a 0→1 transition restarts voices that are
keyed on and looping.

### 4. Address units — the most common misconception

**Address registers do not hold byte addresses.** The high, mid, and low bytes
compose as `<<17`, `<<9`, and `<<1`, so the result is the byte address shifted left
by one, i.e. a nibble-unit address:

```
address_in_nibbles = (high << 17) | (mid << 9) | (low << 1)
byte_address       = address_in_nibbles / 2
```

The playback position uses the same nibble units; only the advance differs by mode:
ADPCM advances 1 per sample and selects a nibble of `read_byte(position / 2)` by
shifting `(~position & 1) << 2` (high nibble first), PCM8 advances 2 and reads
`read_byte(position / 2)`, and PCM16 advances 4 and reads that byte plus the next.

One rule covers all three modes, so any attempt at per-mode address conversion will
diverge.

### 5. Playback frequency

```
source_rate = output_rate * output_step / 512
output_step = (ADPCM ? fnum & 0xFF : fnum & 0x1FF) + 1
output_rate = master_clock / 384 * 2
```

At a 16.9344 MHz clock, `output_rate` is 88200 Hz. ADPCM caps `output_step` at 256
for a 44.1 kHz maximum and PCM at 512 for 88.2 kHz, matching the datasheet exactly.

### 6. ADPCM decode

A single predictor and step size:

```
signal += (step * diff_lookup[nibble]) / 8         // clamp to -32768..32767
step    = (step * index_scale[nibble & 7]) >> 8    // clamp to 0x7F..0x6000
```

with `diff_lookup[n] = (n & 8 ? -1 : 1) * (2 * (n & 7) + 1)`,
`index_scale = {0x0E6, 0x0E6, 0x0E6, 0x0E6, 0x133, 0x199, 0x200, 0x266}`, and
`signal = 0`, `step = 0x7F` on key-on.

**Looping caveat:** the predictor is history dependent, so the `signal` and `step`
observed the first time playback reaches the loop start must be saved and restored
on every wrap. Resetting the predictor instead produces an audible click at the
loop boundary.

### 7. Volume and pan

```
pan == 8 : left = right = level
pan <  8 : left = level,                   right = (pan == 0) ? 0 : level * (pan - 1) / 7
pan >  8 : left = level * (15 - pan) / 7,  right = level
```

Mixing accumulates `interpolated_sample * channel_volume / 2` per voice against a
`32768 * 256` normalization, so the s16 output is the accumulator divided by 256 and
clamped.

### 8. Status register and IRQ

Reaching the stop address sets that voice's bit in the status register. An IRQ
requires both the `0xFE` mask and the `0xFF` IRQ enable. Reading the status register
clears it. On boards that leave the IRQ line unwired, only the status register
matters.

### 9. Implementation in rePIU

The chip core lives in
[src/sound/ymz280b_device.cpp](../../src/sound/ymz280b_device.cpp); the PIU10 board
wiring and port decode are documented in
[docs/analysis/piu-io-port-specification.md](../analysis/piu-io-port-specification.md).
