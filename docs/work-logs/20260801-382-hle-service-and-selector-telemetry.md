# 20260801-382 작업 로그: HLE 서비스 및 selector telemetry / Work Log: HLE service and selector telemetry

설계: [20260801-382-hle-service-and-selector-telemetry.md](../design/20260801-382-hle-service-and-selector-telemetry.md)
작업 지시: [20260801-382-hle-service-and-selector-telemetry.md](../work-orders/20260801-382-hle-service-and-selector-telemetry.md)

## 한국어

### 구현

- `INT 21h` 처리 시 AH별 고정 256칸 카운터를 증가시키고, 종료 로그에 상위 네 서비스를 출력합니다.
- segment load/store는 ES/SS/DS/FS/GS register별 카운터를 수집해 snapshot에 보존합니다.
- Port I/O는 opcode histogram과 input/output, handled/unhandled 합계를 기존 observation에 추가하고 종료 로그에 합계를 출력합니다.
- 모든 카운터는 guest 실행 스레드가 기존 기록 지점에서만 갱신하므로 원자 연산이나 동적 할당을 추가하지 않습니다.

### 검증

- `git diff --check` 통과.
- `cmd /c scripts\\build_win32_x86.bat`는 출력 없이 124초 제한에 도달해 완료하지 못했습니다.
- 기존 `build/win32_x86_debug`에서 `cmake --build ... --target repiu_exe`를 시도했으나 구성 generator가 현재 설치되지 않은 `Visual Studio 18 2026`이라 실행 전 실패했습니다.

다음 Music Select 캡처의 `Win32 DOS AH hotspots`, `Win32 port I/O input/output/handled/unhandled` 로그를 사용해 구현 우선순위를 확정합니다.

## English

### Implementation

- Increment a fixed 256-slot histogram by AH for handled `INT 21h` calls and print the top four services at shutdown.
- Collect segment load/store counters by ES/SS/DS/FS/GS register and preserve them in the result snapshot.
- Add an opcode histogram plus input/output and handled/unhandled totals to the existing Port-I/O observation, and print its totals at shutdown.
- All counters are updated only by the guest execution thread at existing recording sites; no atomics or dynamic allocation are added.

### Verification

- `git diff --check` passed.
- `cmd /c scripts\\build_win32_x86.bat` produced no output and reached the 124-second limit before completion.
- `cmake --build ... --target repiu_exe` from the existing `build/win32_x86_debug` failed before building because it requires an unavailable `Visual Studio 18 2026` generator.

Use the next Music Select capture's `Win32 DOS AH hotspots` and `Win32 port I/O input/output/handled/unhandled` lines to set implementation priority.
