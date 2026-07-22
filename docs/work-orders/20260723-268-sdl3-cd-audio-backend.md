# SDL3 CD-DA 백엔드 작업 지시

1. `waveOut` resource와 WinMM header를 제거하고 SDL3 audio header/stream으로 교체합니다.
2. 기존 CHD/LBA/MSCDEX play-stop-resume 계약을 보존합니다.
3. queue 상한, 오류 telemetry, 종료 순서를 구현합니다.
4. Win32 x86 Debug 단일 병렬 빌드로 검증하고 작업 로그를 남깁니다.

# SDL3 CD-DA Backend Work Order

Replace WinMM/waveOut with an SDL3 audio stream, retain the CHD/LBA/MSCDEX contract, implement bounded queue/error/shutdown behavior, build Win32 x86 Debug serially, and log the result.
