# 20260809-454 PIU10 MP3 SDL3_mixer HLE 작업 지시 / PIU10 MP3 SDL3_mixer HLE Work Order

설계: [20260809-454-piu10-mp3-sdl3-mixer-hle.md](../design/20260809-454-piu10-mp3-sdl3-mixer-hle.md)

## 한국어

### 목표

원본 게임이 PIU10 MAS3507D로 전송하는 MP3 byte stream을 SDL3_mixer로 decode·재생하고
`pumpito`의 8-bit `0x02DA` port access 종료를 제거합니다.

### 작업 항목

- [x] `Piu10IsaBoard`에 8-bit register access와 MP3 byte sink를 추가합니다.
- [x] Win32 port adapter가 PIU10 byte write를 정상 전달하도록 수정합니다.
- [x] SDL3_mixer 3.2.0과 `dr_mp3`만 정적으로 구성하고 license notice를 기록합니다.
- [x] Win32 `Piu10Mp3AudioOut`의 buffer, idle framing, decode, playback, 정리를 구현합니다.
- [x] target-scoped 실행 준비 과정에서 backend와 보드를 연결합니다.
- [x] probe, 전체 Win32 x86 Debug build, `pumpito` runtime을 검증합니다.
- [x] architecture, 누적 분석, 작업 로그를 갱신하고 하나의 Git commit을 남깁니다.

## English

### Objective

Decode and play the original game's PIU10 MAS3507D MP3 byte stream through SDL3_mixer and
remove the eight-bit `0x02DA` termination in `pumpito`.

### Work Items

- [x] Add eight-bit register access and an MP3 byte sink to `Piu10IsaBoard`.
- [x] Make the Win32 port adapter forward PIU10 byte writes normally.
- [x] Configure static SDL3_mixer 3.2.0 with only `dr_mp3` and record license notices.
- [x] Implement Win32 `Piu10Mp3AudioOut` buffering, idle framing, decoding, playback, and cleanup.
- [x] Connect the backend and board during target-scoped execution setup.
- [x] Verify probes, the complete Win32 x86 Debug build, and a `pumpito` runtime.
- [x] Update architecture, cumulative analysis, and the work log, then leave one Git commit.
