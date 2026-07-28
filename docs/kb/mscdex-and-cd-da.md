# MSCDEX와 CD-DA

MSCDEX는 DOS에서 CD-ROM filesystem과 audio control을 제공하는 resident extension입니다. application은 `INT 2Fh AX=1500h`로 drive 수와 첫 drive를 조회하고, `AX=1510h`에서 `ES:BX`의 DOS device request header를 driver에 전달합니다. `1510h`의 carry clear는 “driver가 호출됨”을 뜻하며 실제 성공 여부는 request header status word에서 확인합니다.

```mermaid
sequenceDiagram
    participant App as DOS application
    participant X as MSCDEX INT 2Fh
    participant Driver as CD-ROM device
    App->>X: AX=1500h
    X-->>App: BX=drive count, CX=first drive
    App->>X: AX=1510h, ES:BX=request
    X->>Driver: command packet
    Driver-->>X: packet status/data
    X-->>App: CF clear; inspect status
```

CD-DA는 audio CD의 Red Book PCM 형식입니다. 한 sector는 1/75초이며 588 stereo sample frame, 2352 bytes를 담습니다. 표준 재생 형식은 44.1 kHz, stereo, signed 16-bit PCM입니다. CHD는 CHT2/CHTR metadata로 track type, frame count, pregap을 보존합니다.

주요 DOS device command는 `03h` IOCTL input, `0Ch` IOCTL output, `83h` seek,
`84h` play audio, `85h` stop audio, `88h` resume audio입니다. MSCDEX dispatch 성공,
request status, host audio 상태를 서로 다른 계층으로 구분해야 합니다.

## IOCTL input control block code

`03h` 요청의 transfer buffer 첫 바이트가 subfunction을 정합니다. 위치 조회와
관련된 항목은 다음과 같습니다.

| code | 내용 | 비고 |
|---|---|---|
| `01h` | read head location | `01h` 다음 바이트가 addressing mode(00h HSG / 01h Red Book), 이어서 dword 위치. 재생 위치 폴링의 표준 호출 |
| `06h` | device status | |
| `09h` | media changed | |
| `0Ah` | audio disc information | 최저/최고 track 번호와 lead-out 주소 |
| `0Bh` | audio track information | track 시작 주소와 control byte(`40h` = data track) |
| `0Ch` | audio Q-channel information | 아래 주의 |
| `0Dh` | audio sub-channel information | sub-channel Q를 전송 버퍼로 출력 |
| `0Fh` | audio status information | paused 비트와 **마지막 Play의 시작/끝 주소** |

`0Ch` Q-channel 구조에서 offset 4~6은 **트랙 내 상대 시간**, offset 8~10은
**디스크 절대 시간**입니다. 절대 시간만 lead-in 2초 오프셋(LBA 0 = `00:02:00`)을
가지며, 상대 시간에 이 오프셋을 더하면 항상 2초 앞선 값이 됩니다.

`0Fh`의 offset 3/7은 현재 위치가 아니라 마지막 Play 명령의 시작/끝 주소입니다.

request header offset `12h`의 transfer count는 응답 크기를 정하는 값이 아닙니다.
실제 driver는 control block code로 응답 구조 크기를 정하며, 호출자가 이 필드를
control block보다 짧게 두거나 0으로 두는 경우가 흔합니다. 이 값으로 요청을
거절하면 정상적인 호출까지 `8103h`으로 막게 됩니다. PIU가 `0Ch`을 11 미만의
count로 호출하는 것이 확인된 사례입니다.

## CHD의 pregap 저장 여부

CHD CD metadata(`CHT2`)는
`TRACK:%d TYPE:%s SUBTYPE:%s FRAMES:%d PREGAP:%d PGTYPE:%s PGSUB:%s POSTGAP:%d`
형식입니다. `PGTYPE`가 `V`로 시작하면 pregap 프레임이 그 트랙의 `FRAMES` 안에
실제로 저장되어 있고, 그렇지 않으면 pregap은 논리적 간격일 뿐 파일에 없습니다.
`PGTYPE`를 확인하지 않고 항상 pregap만큼 건너뛰면, 저장되지 않은 pregap을 가진
트랙에서 음악의 앞부분을 그만큼 잘라먹게 됩니다.

또한 CHD는 트랙마다 `CD_TRACK_PADDING`(4) 프레임 경계로 정렬해 저장하므로, 파일
내 프레임 번호는 디스크 논리 LBA와 다릅니다. 게스트에게 보고하는 TOC는 논리
주소여야 하고, 읽기 시점에만 물리 프레임으로 변환해야 합니다.

출처:

* [RBIL INT 2Fh AX=1510h](https://fd.lod.bz/rbil/interrup/io_disk/2f1510.html)
* [RBIL CD-ROM device driver IOCTL input](https://fd.lod.bz/rbil/interrup/cdrom/index.html)
* [MAME chdman documentation](https://docs.mamedev.org/tools/chdman.html)
* 저장소의 libchdr `chd.h`, `cdrom.h` (BSD-3-Clause)

# MSCDEX and CD-DA

MSCDEX is the DOS resident CD-ROM extension. `INT 2Fh AX=1500h` queries installed drives and `AX=1510h` forwards an `ES:BX` request. Carry clear means the driver was called; packet status reports command success. CD-DA stores 588 stereo 16-bit sample frames in each 2352-byte, 1/75-second sector at 44.1 kHz. CHD metadata preserves track types, frame counts, and pregaps.
