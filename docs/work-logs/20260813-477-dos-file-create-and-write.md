# DOS 파일 생성·쓰기 서비스 작업 로그

관련 문서: [설계](../design/20260813-477-dos-file-create-and-write.md),
[작업 지시](../work-orders/20260813-477-dos-file-create-and-write.md)

## 시작점

사용자가 "실행 중 에러"를 보고했습니다. 로그가 사유를 직접 말하고 있었습니다.

```
Win32 minimal execution message: unsupported DOS INT 21h AH=0x3c
Win32 minimal execution exception EIP: 0x040F8417
Relocated exception bytes: ... 89 F2 B4 3C [CD] 21 ...
Win32 exception register string ESI: "ERRLOG.txt....bga\00.dat.font.tg"
```

`MOV AH,3Ch` + `INT 21h`, `DS:EDX = "ERRLOG.txt"`, `CX = 1`(read-only). DOS **Create or
Truncate File**이고 우리 INT 21h 처리 목록에 `3C`가 없었습니다.

**먼저 확인한 것:** 이 구동의 Glide 구현 공백(`repiu-fatal`)은 **0건**입니다. Task 476의
LFB region 작업은 완결되었고 이번 종료는 그와 무관합니다. 게임은 25초를 돌았고
(Glide direct dispatch 성공 295,301회, MP3 194프레임 디코드) 그 뒤에 이 벽을
만났습니다.

## 확인한 것

DOS 가상 파일 시스템은 **읽기 전용**이었습니다. 열기·읽기·탐색·닫기만 있고 생성과 쓰기가
없었으며, `AH=40h`는 **handle을 보지 않고 전량 콘솔로 보낸 뒤 성공을 보고**했습니다.
게스트가 파일에 쓴 내용은 어디에도 남지 않았습니다.

`DosOpenFileHandle::cached_file_size`의 주석이 이 시점을 예고하고 있었습니다 — "Safe to
cache because this API has no write path -- add invalidation here if one is ever
introduced." 이번이 그 지점입니다.

## 한 일

**생성 위치는 게스트 디렉터리.** mount root 아래 게스트가 지정한 경로 그대로 만듭니다.
원본 PIU가 `PIU.EXE` 옆에 `ERRLOG.txt`를 만들던 것과 같고, 게스트가 되읽어도 기존 읽기
경로가 그대로 찾습니다. overlay를 두면 open 해석 순서라는 새 규칙이 생기는데 그것을
정당화할 관측이 아직 없습니다. 대신 성질을 기록해 뒀습니다 — mount cache 무효화 시
`remove_all(mount_root)`로 지워집니다.

**쓰기 가능한 handle.** `DosOpenFileHandle`에 `writable`과 출력 스트림 캐시를
추가했습니다. 읽기 캐시(Task 374)와 같은 복사 규약이며, 쓰기는 `cached_file_size`를
갱신하고 같은 handle의 읽기 캐시를 무효화합니다.

**속성은 호스트에 걸지 않습니다.** 게임은 read-only로 만든 뒤 그 handle로 계속 씁니다.
DOS에서는 정상이지만 호스트에 `FILE_ATTRIBUTE_READONLY`를 걸면 바로 다음 쓰기가
막힙니다. 이미 있던 `attribute_overrides`에 기록해 `AH=43h` 조회만 그 값을 보게 했습니다.

**`AH=40h` 라우팅.** VFS의 열린 쓰기 가능 handle이면 파일로, 그 외에는 기존 콘솔
그대로입니다. VFS handle은 5부터 할당되므로 표준 출력 경로는 영향이 없습니다.

**게임의 오류 보고를 읽을 수 있게 했습니다.** 이것이 이 작업의 실질적 소득입니다.
쓰기를 `dos_file_io` trace에 남기고, 추가로 `[repiu-dos] write handle=N bytes=M "..."`
줄로 본문을 최대 240바이트씩 64회까지 echo합니다. trace prefix는 16바이트뿐이라 문장을
담지 못하기 때문입니다. 생성도 `[repiu-dos] create` 줄로 남깁니다.

## 검증

**`dos_file_create_probe` 8개 항목 전부 통과.** `repiu_aot_probe`가 exit 0으로 끝나므로
기존 probe 회귀도 없습니다(Task 476의 `glide_lfb_region_all=true` 포함).

```
dos_file_create_initialized=true
dos_file_create_created=true
dos_file_create_write=true
dos_file_create_round_trip=true
dos_file_create_truncation=true
dos_file_create_missing_parent=true
dos_file_create_read_only_attribute=true
dos_file_create_exhaustion=true
dos_file_create_all=true
```

probe는 임시 디렉터리를 자기 루트로 쓰므로 실패해도 runtime mount에 아무것도 남기지
않습니다. handle 소진 항목은 5~19번 15개를 모두 잡은 뒤 16번째가 거절되는지 확인합니다 —
Task 228이 남긴 20칸 제한 그대로입니다.

Win32 x86 Debug 전체 빌드 성공.

**미수행:** `pumpit8` 실물 구동 확인은 사용자 몫입니다.

## 남은 것

* **게임이 `ERRLOG.txt`에 무엇을 쓰는지가 다음 단서입니다.** 다음 구동 로그의
  `[repiu-dos] write` 줄 또는 `build/runtime_mounts/pumpit8/PIU/ERRLOG.txt` 파일을 보면
  게임이 무엇을 오류로 판단했는지 직접 알 수 있습니다.
* 게임이 이 파일을 항상 만드는지, 오류를 감지했을 때만 만드는지는 미확정입니다.
* `AH=5Bh`(create new), `41h`(delete), `56h`(rename), `39h`/`3Ah`(mkdir/rmdir)는
  관측되지 않아 제외했습니다. 나타나면 같은 계층에 추가하면 됩니다.

# DOS File Create and Write Work Log

## Starting point

The user reported an error during execution, and the log named it directly:
`unsupported DOS INT 21h AH=0x3c`, with the bytes at `0x040F8417` decoding to
`mov ah,3Ch` / `int 21h`, `DS:EDX` pointing at `"ERRLOG.txt"`, and `CX = 1`
(read-only) — DOS **Create or Truncate File**, absent from the handled INT 21h
set. Checked first: this run recorded **zero** Glide implementation gaps, so Task
476's LFB region work is complete and unrelated. The game ran 25 seconds, with
295,301 successful Glide direct dispatches and 194 MP3 frames decoded, before
hitting this wall.

## What the investigation confirmed

The DOS virtual file system was read-only — open, read, seek, close — and
`AH=40h` ignored the handle, sent everything to the console, and reported
success, so nothing a guest wrote was ever kept.
`DosOpenFileHandle::cached_file_size` even carried the note that caching was safe
only while no write path existed; this task is that point.

## What was done

Created files land under the mount root at the guest's own path, matching what
the original PIU did next to `PIU.EXE` and letting the unchanged read path find
them again; an overlay would add an open-resolution order no observation yet
justifies, and the property that a mount-cache invalidation removes them is
recorded rather than worked around. `DosOpenFileHandle` gained a `writable` flag
and an output stream cache following the read cache's copy contract (Task 374),
with writes updating `cached_file_size` and invalidating the read cache on the
same handle. The creation attribute goes into `attribute_overrides` instead of
onto the host file, because the guest creates read-only and then writes through
that handle — legal in DOS, blocked by a host read-only attribute. `AH=40h` now
routes writable VFS handles to their file and leaves every other handle,
standard handles 0-4 included, on the console path it has always had.

The practical prize is that the game's own error report becomes readable: writes
are traced, and because the trace prefix holds only 16 bytes, up to 64 writes are
also echoed as `[repiu-dos] write` lines carrying up to 240 bytes of text.

## Verification

All eight `dos_file_create_probe` checks pass and `repiu_aot_probe` exits 0, so
no existing probe regressed, Task 476's included. The probe roots itself in a
temporary directory, so a failed run leaves nothing in a runtime mount, and the
exhaustion check holds all fifteen handles 5..19 before confirming the sixteenth
is refused — the 20-slot limit Task 228 established. The full Win32 x86 Debug
build succeeds. Running `pumpit8` against it is left to the user.

## Remaining

What the game writes into `ERRLOG.txt` is the next lead: the `[repiu-dos] write`
lines in the next run log, or the file at
`build/runtime_mounts/pumpit8/PIU/ERRLOG.txt`, say directly what the game
considers an error. Whether it creates that file unconditionally or only on
detecting a fault is still unknown. Create-new (`5Bh`), delete, rename, and
mkdir/rmdir were excluded for lack of any observation and belong in the same
layer if they appear.
