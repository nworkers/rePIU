# CAT702 PIU 직렬 보안 장치 / CAT702 PIU Serial Security Device

## 한국어

CAT702는 8-bit 내부 상태와 칩별 8-byte linear transform을 사용하는 직렬 보안
장치입니다. 일반 CAT702 통신은 PlayStation controller serial protocol과 유사하지만,
Pump It Up은 data, clock, select 선을 직접 구동하는 별도 clock 순서를 사용합니다.

PIU 변형은 select가 low가 될 때 내부 상태를 `0xFC`로 초기화하고 고정 initial S-box를
적용합니다. select가 low인 동안 clock rising edge마다 다음 순서로 진행합니다.

1. data-in이 0이면 현재 bit 위치에 대응하는 칩별 linear transform을 적용합니다.
2. bit 위치를 0..7 범위에서 하나 증가시킵니다.
3. 8-bit 경계라면 fixed initial S-box를 다시 적용합니다.
4. 새 bit 위치의 내부 상태 bit를 data-out으로 내보냅니다.

칩별 8-byte transform은 게임 타깃의 보안 데이터이므로 코드 상수가 아니라 ROM-set
자산에서 읽어야 합니다. 장치 모델은 입력 명령의 결과를 미리 정해 반환하는 게임별
우회가 아니라, 실제 직렬 상태 전이를 보존해야 합니다.

권위 있는 구현 참고 자료:

- MAME [CAT702 device](https://github.com/mamedev/mame/blob/master/src/devices/machine/cat702.cpp)
- MAME [PIU10 ISA board](https://github.com/mamedev/mame/blob/master/src/mame/misc/xtom3d_piu10.cpp)

## English

CAT702 is a serial security device with an eight-bit internal state and a chip-specific
eight-byte linear transform. Ordinary CAT702 communication resembles the PlayStation controller
serial protocol, while Pump It Up drives the data, clock, and select lines directly with a
different clock sequence.

The PIU variant initializes the state to `0xFC` and applies a fixed initial S-box when select
goes low. On every rising clock edge while selected, it applies the bit-position-specific linear
transform when data-in is zero, advances the bit position modulo eight, reapplies the fixed S-box
at an eight-bit boundary, and exposes the new state bit on data-out.

The chip-specific transform is target security data and should be loaded from the ROM-set asset,
not embedded as a program constant. A device model should preserve these serial state transitions
rather than returning a title-specific predetermined answer.
