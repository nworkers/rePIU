# 350 MSCDEX 위치 보고 및 CHD 논리 주소 공간 구현 계획

설계: `docs/design/20260729-350-mscdex-cd-position-and-toc.md`

## 범위

1. CHD 논리/물리 주소 공간 분리 및 `PGTYPE` 존중
2. MSCDEX IOCTL 미구현/오구현 보완
3. 재생 위치를 실제 가청 위치로 산출
4. IOCTL subfunction 및 Play 요청 진단 텔레메트리

## 단계

### 1. `repiu_chd_cd_probe` 진단 확장 (완료)

* `--metadata`로 원본 CHT2 문자열 출력
* `--scan <track>`로 저장 구간 진폭 스캔 및 첫 가청 프레임 보고
* 검증: 트랙 2 `PGTYPE:MODE1`, 첫 가청 프레임 offset 1 확인

### 2. `ChdCdImage` 주소 공간 분리

* `include/repiu/media/chd_cd_image.h`
  * `ChdCdTrack`에 `physical_lba`, `stored_frames`, `pregap_in_file`,
    `logical_lba`, `data_start_lba`, `metadata` 추가
  * `start_lba`/`end_lba`의 의미를 논리 주소로 확정
* `src/media/chd_cd_image.cpp`
  * `PGTYPE` 첫 글자가 `V`인지로 pregap 저장 여부 판정
  * 논리/물리 커서를 각각 누적
  * `ReadRawSector`는 논리 LBA를 받아 물리로 변환, 미저장 pregap은 무음 반환
  * `lead_out_lba`는 논리 공간 기준
* 검증: probe로 트랙 2 논리 시작 7104, 물리 6956 확인

### 3. `CdAudioWaveOut` 가청 위치와 상태

* 큐 투입 커서와 별도로 `SDL_GetAudioStreamQueued` 기반 가청 LBA 산출
* `Stop()`은 위치 보존 정지, `Pause()`/`Resume()` 분리
* `playing()`은 큐가 비워질 때까지 유지
* 마지막 Play 구간(`last_play_start_lba`, `last_play_end_lba`) 보존

### 4. MSCDEX 보완

* `src/platform/win32/dos/dpmi_mscdex_services.cpp`
  * IOCTL `01h` read head location (addressing mode 존중)
  * IOCTL `0Dh` audio sub-channel info
  * IOCTL `0Ch` 트랙 상대시간에서 150프레임 오프셋 제거, ADR nibble 보정
  * IOCTL `0Fh` offset 3/7을 마지막 Play 구간으로, paused 비트 정정
  * device command `83h` seek, `0Ch` IOCTL output
  * `85h` 위치 보존 정지, `88h` 재개

### 5. 진단 텔레메트리

* `include/repiu/platform/win32/live_telemetry.h`
  * `mscdex_last_ioctl_subfunction`, `mscdex_ioctl_reject_mask`
  * `mscdex_last_play_mode`, `mscdex_last_play_start`, `mscdex_last_play_length`
* `thread_context.h`, `live_telemetry_snapshot.cpp`, `src/host/win32/main.cpp`
  최종 요약에 반영

### 6. 검증

* `cmake --build build --config Release`
* `repiu_chd_cd_probe --metadata`, `--scan 2`, `--scan 3`
* 구동 로그에서 신규 텔레메트리 확인 (사용자 구동)

## 문서

* `docs/analysis/pumpit1-mscdex-cd-audio.md` 갱신
* `docs/kb/mscdex-and-cd-da.md`에 IOCTL subfunction 표와 CHD pregap 규약 추가
* `docs/work-logs/20260729-350-*.md` 작성

---

# 350 Implementation Plan

Scope: split CHD logical and physical address spaces while honoring `PGTYPE`,
fill the MSCDEX IOCTL gaps, derive playback position from actually-consumed
audio, and add telemetry that identifies which IOCTL subfunction and play
request the game issues.

Steps run in order: probe diagnostics (done), `ChdCdImage` address split,
`CdAudioWaveOut` audible position and pause/stop states, MSCDEX subfunction and
device command coverage, telemetry plumbing through `live_telemetry.h`,
`thread_context.h`, the snapshot, and the host summary, then a Release build plus
probe re-verification. Analysis, knowledge base, and work log documents are
updated in the same task.
