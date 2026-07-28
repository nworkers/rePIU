# 350 MSCDEX 위치 보고 및 CHD 논리 주소 공간 작업 로그

* 설계: `docs/design/20260729-350-mscdex-cd-position-and-toc.md`
* 계획: `docs/work-orders/20260729-350-mscdex-cd-position-and-toc.md`
* 브랜치: `feature/350-mscdex-position-and-cd-toc`
* 커밋: `7dad3df`

## 요구사항

1. CD 재생 중 위치 정보가 제대로 전달되지 않는 문제의 명확한 미구현/오구현 수정
2. 진단 수단 확보
3. `pumpit1` 인트로에서 트랙 2 음악이 처음이 아니라 중간부터 시작하는 문제 원인 규명

## 진행

### 진단 도구 확장

`repiu_chd_cd_probe`에 `--metadata`(원본 CHT2 문자열)와 `--scan <track>`(저장 구간
진폭 스캔, 첫 가청 프레임 보고)을 추가했다. CD-DA가 CHD에 big-endian으로 저장되는
점을 반영해 peak 계산 시 샘플 바이트를 교환한다.

### 트랙 2 근인 확정

```
TRACK:2 TYPE:AUDIO FRAMES:820 PREGAP:150 PGTYPE:MODE1     <- V 접두어 없음
TRACK:3 TYPE:AUDIO FRAMES:1983 PREGAP:149 PGTYPE:VAUDIO
```

`PGTYPE`에 `V`가 없으면 pregap이 파일에 저장되어 있지 않다. `ChdCdImage::Open`은
`PGTYPE`를 보지 않고 항상 pregap만큼 건너뛰었으므로 트랙 2에서 음악 앞
150프레임(정확히 2.00초)을 잘라먹었다.

`--scan 2`가 저장 시작 직후 섹터에서 peak 32604를 보고해 무음 pregap이 없음을
확인했고, `--scan 3`은 머리 184프레임이 무음이어서 대조군으로 성립했다. 영향받은
트랙은 트랙 2 하나뿐이다.

### 물리/논리 주소 공간 분리

트랙 2의 실제 논리 INDEX 01은 7104인데 기존 보고값은 7106이었다. 두 값이 근접해서
게임이 우리 TOC를 되읽는지 원본 디스크 주소를 내장하는지 구분할 수 없었고, 두
경우 모두 약 2초 밀린 지점을 가리켰다. `PGTYPE` 수정만으로는 후자를 못 고치므로
주소 공간을 분리했다.

`ChdCdTrack`이 `physical_lba`/`stored_frames`(물리)와
`logical_lba`/`data_start_lba`/`start_lba`/`end_lba`(논리)를 함께 갖고,
`ReadRawSector`는 논리 LBA를 받아 물리로 변환하며 저장되지 않은 pregap 구간은
디지털 무음을 반환한다. `FindTrackByLba`는 pregap을 포함한 논리 전체 구간을
매칭한다.

검증:

```
track=2 phys=6956 frames=820 logical=6954 data_start=7104 start=7104 end=7924
first_audible_lba=7105  offset_from_reported_start=1
track=3 phys=7776 logical=7924 data_start=7924 start=8073 end=9907
first_audible_lba=8108  offset_from_reported_start=35
lead_out=258684 (이전 물리값 258607)
```

논리 7104가 물리 6956에 매핑되어 음악 첫 프레임을 가리킨다.

### MSCDEX 보완

* IOCTL `01h` read head location 구현. caller가 지정한 addressing mode를 존중한다.
* IOCTL `0Ch` Q-channel: offset 4~6 상대 시간에서 150프레임 가산 제거
  (`MscdexFramesToMsf` 신설), ADR nibble 설정, pregap 구간에서 index 0과 카운트다운
  처리.
* IOCTL `0Fh`: offset 3/7을 마지막 Play의 시작/끝 주소로 정정, paused 비트를 실제
  일시정지 상태에서 유도.
* device command `83h` seek, `0Ch` IOCTL output(트레이/도어/볼륨은 이미지 백엔드에서
  의미 없는 no-op으로 수락) 추가.
* `85h`는 위치를 보존하는 정지로, `88h`는 그 위치에서 재개로 동작.

### 재생 위치 산출

`current_lba`가 `SDL_PutAudioStreamData` 직후 갱신되는 디코드 커서였다. 큐 임계값
32섹터(426.7 ms), 투입 단위 8섹터(106.7 ms)라서 가청 위치보다 320~427 ms 앞서고
계단식으로 튀었다.

`SDL_GetAudioStreamQueued`로 미소비 바이트를 빼서 가청 위치를 만들되, 게스트가
VEH 핸들러 안에서 이 값을 읽으므로 **SDL 락을 게스트 스레드가 잡지 않도록** 워커
스레드에서만 샘플링해 atomic에 적재하고 `current_lba()`는 그 atomic만 읽는다.

`playing`은 큐가 완전히 비워질 때까지 유지해 꼬리 0.4초가 잘리지 않게 했다.
Play/Stop/Seek는 generation 카운터를 올리고 워커는 커밋 직전 generation을 비교해,
디코드 도중 상태가 바뀐 버퍼가 커서를 덮어쓰지 않게 했다.

### 진단 텔레메트리

`mscdex_last_ioctl_subfunction`(하위 바이트 = code, bit 8 = 처리 여부),
`mscdex_ioctl_reject_mask`, `mscdex_last_play_mode/start/length`,
`mscdex_last_seek_target`, `cd_audio_reported_lba`를 live telemetry와 attempt
snapshot에 추가하고 loader 최종 요약에 두 줄을 출력한다.

## 검증

* `cmake --build build --config Release` 전체 성공 (오류 0)
* `ctest`는 등록된 테스트 없음
* probe로 TOC 산술과 트랙 2/3 첫 가청 프레임 재확인 (위 수치)
* 게임 실구동 검증은 미수행. 텔레메트리 확인은 사용자 구동 로그 필요

빌드 트리 재구성 시 SDL3 `FetchContent`가 네트워크를 요구해 실패했다.
`-DFETCHCONTENT_FULLY_DISCONNECTED=ON`으로 기존 populate 내용을 재사용했다.

## 후속: 실구동 로그 분석 (커밋 `9d103fb`)

사용자 제공 `repiu_log.txt`(UTF-16LE, 02:08~02:10 구동)에서 위치가 여전히 갱신되지
않는 원인을 확정했다.

```
MSCDEX IOCTL last subfunction/handled/reject mask: 0x0000000C/false/0x00001000
MSCDEX last play mode/start/length/seek target: 0/163433/11899/0
MSCDEX available/audio/tracks/requests/current LBA: true/true/51/2679/166629
```

* PIU는 `0Ch`(Q-channel)로 위치를 폴링한다. `01h`은 쓰지 않는다.
* reject mask가 bit 12 단독, `handled=false` → 거절되는 subfunction은 위치 조회
  하나뿐이었다.
* 근인은 `HandleMscdexIoctl`이 request transfer count를 그대로 넘기고 `case 12`가
  `length < 11`로 거절한 것. PIU는 11 미만을 선언한다. 이 gate는 Task 350 이전부터
  있었으므로 위치는 처음부터 한 번도 전달된 적이 없다.
* 수정: 거절 기준을 실제 writable로 검증한 용량(≥16바이트)으로 바꾸고, 선언된
  count는 텔레메트리에만 기록.

부수 확인으로 논리 주소 전환이 옳게 먹은 것도 증명됐다. play 요청
`start=163433 length=11899`가 트랙 24의 `start_lba`/`frames`와 정확히 일치하므로
게임은 우리 TOC를 되읽어 재생한다. 내부 위치도 166629(재생 시작 +3196프레임,
42.6초)까지 정상 전진해 있었다.

또한 loader 요약 format string에 백슬래시를 써서 `\reject mask`의 `\r`이 carriage
return이 되어 로그 한 줄이 쪼개져 있었다. 슬래시로 교정했다.

사용자가 실행하는 트리는 `build/win32_x86_debug`이므로 Release와 함께 그쪽도
빌드했다.

## 남은 과제

* PIU가 실제로 쓰는 IOCTL subfunction과 play addressing mode를 구동 로그로 확정
* IOCTL `0Dh` audio sub-channel info는 의도적으로 미구현. 형식 기대치를 틀리면
  조용히 잘못된 데이터를 주게 되므로, 게임이 실제로 호출하는 것이 reject mask로
  확인되면 그때 구현한다.
* 우리 논리 TOC와 원본 디스크 TOC의 track별 대조

---

# 350 Work Log

## Requirement

Fix the clearly unimplemented or misimplemented MSCDEX position reporting, add
diagnostics, and find out why the `pumpit1` intro music (track 2) starts partway
in.

## What was done

The probe gained `--metadata` and `--scan`, which established the root cause:
track 2 carries `PREGAP:150 PGTYPE:MODE1`, and without a `V` prefix those 150
frames are not stored in the file. `ChdCdImage::Open` ignored `PGTYPE` and always
skipped the pregap, cutting exactly 2.00 seconds off a track that is only 10.93
seconds long. `--scan 2` shows full-scale audio one sector after the stored
start; `--scan 3`, the control case, shows 184 silent frames. Track 2 was the
only affected track.

Because track 2's true logical INDEX 01 (7104) sat within two frames of the old
reported value (7106), it was impossible to tell whether the game replays our TOC
or carries hardcoded disc addresses, and both readings land ~2 s late. Physical
and logical address spaces were therefore separated: tracks keep a physical
origin plus a logical extent, the guest only sees logical addresses, and reads
translate back and return silence for pregaps that were never stored.

MSCDEX gained IOCTL `01h`, a corrected `0Ch` relative time via a new
`MscdexFramesToMsf`, spec-accurate `0Fh` fields with a real paused state, and
device commands `83h` and `0Ch`. `85h` and `88h` now preserve and resume from a
position.

Playback position is derived from what SDL has consumed rather than from the
decode cursor, sampled on the worker thread so the guest's VEH handler never
takes SDL's stream lock, and `playing` stays asserted until the queue drains. A
generation counter keeps a mid-flight decode from clobbering the cursor after
Play/Stop/Seek.

Telemetry now records the last IOCTL subfunction and whether it was handled, a
rejected-subfunction mask, and the last play and seek requests.

## Verification

Full Release build clean; no registered tests; probe re-verified the TOC
arithmetic and first audible frames. The game itself was not run, so the new
telemetry still needs a live run to read back. IOCTL `0Dh` is deliberately left
unimplemented rather than fabricated, and will show up in the reject mask if the
game depends on it.
