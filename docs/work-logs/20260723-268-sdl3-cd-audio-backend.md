# SDL3 CD-DA 백엔드 작업 로그

`CdAudioWaveOut`의 guest-facing API와 CHD/LBA 재생 상태를 유지한 채 내부 출력을 WinMM `waveOut`에서 SDL3 `SDL_AudioStream`으로 교체했습니다.

* Open 시 기본 playback device에 44.1 kHz, stereo, S16LE stream을 생성합니다.
* worker는 8 CD-DA sector 단위로 CHD raw PCM을 읽고 16-bit sample byte order를 변환한 뒤 SDL queue에 넣습니다.
* queued PCM은 네 chunk로 제한합니다.
* play/stop/resume/close는 stream queue와 device pause/resume을 갱신하며, close는 worker join 후 stream을 destroy합니다.

검증으로 `cmake --build build\\win32_x86_debug --config Debug -- /m:1`을 실행했고 Win32 x86 Debug library, loader, supervisor가 모두 성공했습니다. SDL public header에서 CP949 C4819 경고가 있었지만 새 backend의 compile/link error는 없었습니다. 실제 PIU의 CD play packet은 기존 분석에서 아직 관측 미완료이므로 audible output 검증은 이후 장시간 실행에서 수행해야 합니다.

# SDL3 CD-DA Backend Work Log

Replaced WinMM `waveOut` output with an SDL3 `SDL_AudioStream` while retaining the guest-facing API and CHD/LBA playback state. The worker queues byte-swapped 44.1 kHz stereo S16LE CD-DA PCM in bounded chunks; play/stop/resume/close update queue and device state, and close joins before stream destruction.

The serialized Win32 x86 Debug build completed successfully for the library, loader, and supervisor. SDL headers emitted only CP949 C4819 warnings. Audible output remains pending because a real PIU play packet has not yet been observed.
