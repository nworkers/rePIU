# 20260729-349 PIT 채널 0 타이머 HLE 작업 로그 / Work log

## 한국어

### 결과

`PIU.EXE`가 설정한 PIT 채널 0 분주값을 보존하고 그 값으로 IRQ0 게시 주기를
계산하도록 수정했습니다. 고정 `55ms` INT 8 스케줄은 제거했습니다.

- 공용 `hle::PitChannel0`이 포트 `0x43` 제어어와 포트 `0x40`의 low/high reload
  바이트를 처리합니다.
- 완성된 설정은 `generation + divisor` 원자 snapshot으로 Win32 폴러에 전달됩니다.
- 공용 `PitIrqSchedule`이 단조 증가 시계와 `1,193,280 / divisor` 비율로 만료 IRQ
  수를 계산합니다.
- BDA `0x46C`는 기본 divisor `65536`의 BIOS tick으로 별도 갱신합니다.
- PIT 출력과 PIC EOI는 공유 Watcom `OUT DX,AL` helper를 NOP으로 바꾸지 않고
  EIP만 전진시킵니다.
- 기존 pending 병합, IF gate, AOT safe point, 원본 INT 8 ISR과 `IRETD`는
  그대로 유지했습니다.

### 원본 설정 확인

정적 역어셈블 결과:

```text
0x030250C0  mov eax, 240
0x030250C5  call 0x030430B0

0x03042FF9  mov edx, 0x36
0x03042FFE  mov eax, 0x43
...
0x0304300C  mov eax, 0x40  ; low byte
...
0x0304301A  mov eax, 0x40  ; high byte
```

기준 상수 `1,193,280.0f`와 요청값 `240`은 reload `4,972`를 만들며,
`1,193,280 / 4,972 = 240Hz`입니다.

### 검증

1. `cmd /c scripts\build_win32_x86.bat`: 성공.
2. 전체 `repiu_aot_probe PIU.EXE`: 성공.
   - `pit_timer_probe=true,divisor=4972,frequency_hz=240`
   - `timer_safe_point_probe=true`
   - `cache_round_trip=true`
3. 실제 `pumpit1` 50초 `aot-dbt` 실행:
   - `[repiu-pit] channel=0 divisor=4972 frequency=240.000000Hz generation=2`
   - 50초까지 heartbeat/dispatch/progress가 계속 증가
   - Glide open/texture/draw/swap 전부 도달
   - 관찰된 `fatal_count/msg=0/0x0`
   - supervisor가 계획된 50초 제한으로 child를 종료
4. 15초 진단 실행에서 guest-thread INT 8 주입 로그 991건, 마지막 fatal 0건을
   확인했습니다. 상세 로그 자체가 실행을 느리게 하고 IF/ISR 실행 중 요청은 기존
   정책대로 병합되므로 이 수는 주파수 측정값이 아니라 전달 경로 생존 확인값입니다.

### 참고

첫 빌드 호출은 전체 재구성 중 120초 도구 제한에 걸렸지만 컴파일과 링크는 계속
진행되어 주요 target이 생성됐고, 바로 이어진 증분 전체 빌드는 16.1초에 성공했습니다.
기존 소스와 third-party header의 C4819 경고 및 Zydis LNK4217 경고는 이전과 같은
비차단 경고입니다.

---

## English

### Result

The fixed `55ms` INT 8 schedule is removed. The HLE now preserves the PIT
channel-0 divisor written by `PIU.EXE` and derives IRQ0 cadence from it.

Shared `hle::PitChannel0` decodes ports `0x43` and `0x40`, publishes an atomic
generation-plus-divisor snapshot after a complete reload, and
`PitIrqSchedule` computes expirations from monotonic time and
`1,193,280 / divisor`. BDA `0x46C` remains a separate BIOS-rate counter using
default divisor `65536`. PIT writes and PIC EOI advance EIP without erasing
the shared Watcom `OUT DX,AL` helper. Existing pending coalescing, IF gating,
AOT safe points, the original INT 8 ISR, and IRETD remain unchanged.

Static disassembly confirmed the game requests `240`, writes control `0x36`,
and emits the two divisor bytes for reload `4,972`, exactly `240Hz`.

The Win32 x86 Debug build and full AOT probe passed, including
`pit_timer_probe=true,divisor=4972,frequency_hz=240`. A real 50-second
`aot-dbt` run logged the exact PIT configuration, continued heartbeat,
dispatch, and progress through timeout, reached Glide open/texture/draw/swap,
and retained zero fatal events. A log-heavy 15-second diagnostic delivered
991 guest-thread INT 8 entries with zero fatal events; this is a delivery-path
check rather than a frequency measurement because logging overhead and the
existing IF/ISR pending coalescing intentionally reduce delivered entries.
