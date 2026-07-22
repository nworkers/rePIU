# SDL3 창 및 CD-DA 오디오 전환 작업 로그

## 완료한 기반 작업

* SDL 3.4.10을 `FetchContent`의 shallow, 고정 tag 의존성으로 추가했습니다.
* `repiu_exe`가 `SDL3::SDL3-static`에 링크되도록 구성했습니다.
* SDL의 zlib 라이선스와 SDL3 OpenGL/audio API 근거를 설계 문서에 기록했습니다.
* 기존 `waveOut`/WGL backend의 실제 전환은 후속 구현 단위로 남겼습니다. SDL video API는 생성 thread 소유권을 요구하므로, guest worker와 loader host thread의 UI/GL command handoff를 먼저 확정하지 않은 상태에서 직접 치환하지 않았습니다.

## 검증

* 기본 빌드에서 SDL3 source fetch 및 `SDL3-static.lib` 생성이 성공했습니다.
* 병렬 MSVC 빌드는 기존 공유 `repiu_exe.pdb` 충돌(C1041)로 실패했습니다. SDL3 컴파일 자체는 성공했으며 이 실패는 SDL source 또는 link error가 아닙니다.
* `cmake --build build\\win32_x86_debug --config Debug -- /m:1`로 단일 병렬 재검증하여 `repiu_exe`, loader, supervisor를 포함한 Win32 x86 Debug 전체 빌드가 성공했습니다.

## 다음 구현 단위

1. SDL video/OpenGL owner 및 guest worker command handoff를 구현합니다.
2. WGL window/context, message pump, GLSL proc resolver를 SDL3 video/OpenGL API로 전환합니다.
3. `CdAudioWaveOut`를 SDL `AudioStream` producer/queue 구현으로 교체하고 실제 CHD 재생을 검증합니다.

# SDL3 Window and CD-DA Audio Migration Work Log

## Completed foundation work

* Added SDL 3.4.10 as a shallow, pinned FetchContent dependency.
* Linked `repiu_exe` with `SDL3::SDL3-static`.
* Recorded zlib licensing and SDL3 OpenGL/audio API references in the design.
* Deferred direct WGL/waveOut replacement until SDL video/GL thread ownership and worker-to-host command handoff are explicitly implemented.

## Verification

SDL3 fetched and built successfully. The normal parallel MSVC build hit the pre-existing shared-PDB C1041 race, not an SDL failure. A serialized `/m:1` Win32 x86 Debug build completed successfully for the library, loader, and supervisor.

## Next unit

Implement SDL video/OpenGL ownership and command handoff, migrate WGL/event/proc-resolution code, then replace CD-DA `waveOut` with queued SDL `AudioStream` output and validate it from the real CHD.
