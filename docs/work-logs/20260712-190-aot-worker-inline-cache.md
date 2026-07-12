# AOT worker 기반 inline cache 작업 로그

## 구현

* platform-neutral code-cache image에 call/jump/return inline-cache site metadata를 추가했습니다.
* prefix 없는 legacy-32 `FF /2`, `FF /4`, `C3`, `C2 iw`를 EFLAGS 보존 guard slot으로 발행했습니다.
* ESP 기반 ModRM/SIB operand는 `pushfd`에 따른 4바이트 stack 이동을 보정했습니다.
* 정적 placement와 동적 append가 site offset을 보존하도록 했습니다.
* Win32 worker request를 translation과 patch operation으로 분리했습니다.
* worker가 RX→RW→patch→RX와 `FlushInstructionCache`를 수행하도록 했습니다.
* 실제 `INT3` 주소가 miss `popfd` 다음 바이트일 수 있어 두 cache 위치를 같은 site로 인식하도록 수정했습니다.
* patch attempt/success, site 수, 마지막 cache boundary telemetry를 추가했습니다.

## 검증

`scripts/build_win32_x86.bat`가 성공했고 `repiu_exe`, `repiu_aot_probe`, loader
및 supervisor target을 모두 생성했습니다. `repiu_aot_probe
MASTER\\PIU_1ST\\PIU\\PIU.EXE`는 `cache_valid=true`, cache decode failure 0,
placement/round-trip true를 기록했습니다.

3초 `pumpit1` AOT 관찰에서 indirect dispatcher는 약 15,327회에서 33회,
return dispatcher는 약 21,894회에서 1,089회로 감소했습니다. 1,122개 patch
요청이 모두 성공했고 heartbeat는 약 4,238에서 약 89,502로 증가했습니다.

30초 관찰은 예외와 legacy fallback 없이 heartbeat 약 1,248,228과 progress 약
145,963까지 진행했습니다. dispatcher/patch 수가 초기 학습 이후 증가하지 않아
반복 경계 비용이 제거됐음을 확인했습니다.

## 다음 frontier

LINEXE service 5가 `_GRGLIDEINIT@0`을 반복해서 성공적으로 조회하지만 Glide
gate로 전송되지 않습니다. 다음 작업은 service 5 output pointer, wrapper 공통
epilogue, AOT 반환 제어 흐름을 legacy와 비교하는 것입니다.

# AOT Worker-backed Inline Cache Work Log

The code cache now emits guarded monomorphic slots for supported indirect calls,
jumps, and returns. Static and dynamic placements preserve site metadata, while
the serialized Win32 worker performs W^X patch publication and instruction-cache
flushes. ESP-based operands and EFLAGS are preserved, and changed targets fail
closed to the existing dispatcher.

The Win32 targets and AOT probe built successfully. Three-second PIU observation
reduced indirect dispatch from about 15,327 to 33 and return dispatch from about
21,894 to 1,089, with 1,122 successful patches. A 30-second run remained free of
exceptions and legacy fallback. The next frontier is the repeated LINEXE service
5 GETPROCADDR return path.
