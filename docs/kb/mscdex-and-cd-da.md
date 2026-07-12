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

주요 DOS device command는 `03h` IOCTL input, `84h` play audio, `85h` stop audio, `88h` resume audio입니다. MSCDEX dispatch 성공, request status, host audio 상태를 서로 다른 계층으로 구분해야 합니다.

출처:

* [RBIL INT 2Fh AX=1510h](https://fd.lod.bz/rbil/interrup/io_disk/2f1510.html)
* [MAME chdman documentation](https://docs.mamedev.org/tools/chdman.html)
* 저장소의 libchdr `chd.h`, `cdrom.h` (BSD-3-Clause)

# MSCDEX and CD-DA

MSCDEX is the DOS resident CD-ROM extension. `INT 2Fh AX=1500h` queries installed drives and `AX=1510h` forwards an `ES:BX` request. Carry clear means the driver was called; packet status reports command success. CD-DA stores 588 stereo 16-bit sample frames in each 2352-byte, 1/75-second sector at 44.1 kHz. CHD metadata preserves track types, frame counts, and pregaps.
