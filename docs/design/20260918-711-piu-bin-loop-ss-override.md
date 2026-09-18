# Task 711 설계 — PIU.BIN 읽기 루프의 근인 추적

## 목적

Task 710이 타이머 가설을 반증한 뒤 남은 질문은 하나다. **Win32는 PIU.BIN 0바이트
읽기 루프를 37회 만에 빠져나오는데 Linux x64는 왜 못 빠져나오는가.** 추측하지 않고
게스트 코드에서 루프의 탈출 조건을 직접 읽는다.

## 코드 변경

게스트 코드를 읽는 데 필요한 관측 한 가지만 바꾼다. DOS file I/O trace의 게스트 스택
캡처를 8 dword에서 24 dword로 넓힌다.

8 dword는 C 런타임의 자기 프레임 — read wrapper, 버퍼 fill, `fread`의 저장
레지스터 — 에서 끝나고, `fread`가 게임 코드로 돌아가는 반환 주소보다 6 dword 모자란다.
파일 읽기 루프가 실제로 사는 곳은 그 프레임이다. 채우기와 보고가 모두 배열 크기를
따르므로 `DosFileIoTraceEntry::guest_stack`의 크기만 바꾸면 된다.

## 방법

1. `PIU.EXE`의 LE 헤더를 읽어 object 2(게스트 코드)를 추출한다. page가 순차라는
   가정은 read 호출 지점이 `mov ah,3Fh; int 21h`로 해독되는 것으로 확인한다.
2. `objdump -b binary -m i386 --adjust-vma=0x01010000`로 호출자 체인을 읽는다.
3. 넓힌 스택 캡처로 `fread`의 반환 주소를 얻어, 정적으로 찾은 `fread` 호출자 75곳
   가운데 루프를 가진 곳을 특정한다.
4. 가설은 **실행으로 판정한다.** 이번에는 인과를 주장하기 전에 커밋하지 않는
   실험으로 먼저 확인한다.

## 검증

* Linux x64, Win32 x86 core probe 전체
* 넓힌 캡처로 `fread` 반환 주소가 보이는지

---

## English

### Purpose

After Task 710 falsified the timer hypothesis, one question remains: **why does
Win32 leave the PIU.BIN zero-length read loop after 37 reads while Linux x64
never does?** Instead of guessing, read the loop's exit condition from the guest
code.

### Code change

Only the observation needed to read the guest code changes: the DOS file-I/O
trace's guest stack capture widens from 8 dwords to 24. Eight stopped inside the
C runtime's own frames — the read wrapper, the buffer fill, and `fread`'s saved
registers — six dwords short of `fread`'s return into the game, which is the
frame the reading loop lives in. The fill and the report both take the array's
size, so only `DosFileIoTraceEntry::guest_stack` changes.

### Method

1. Parse `PIU.EXE`'s LE header and extract object 2 (guest code); confirm the
   sequential-page assumption by decoding the read site as
   `mov ah,3Fh; int 21h`.
2. Read the caller chain with `objdump -b binary -m i386
   --adjust-vma=0x01010000`.
3. Use the widened capture to obtain `fread`'s return address and single out, among
   the 75 statically found `fread` callers, the one that holds the loop.
4. **Settle the hypothesis by running it**, this time with an uncommitted
   experiment before any causal claim.

### Verification

* Every core-probe group on Linux x64 and Win32 x86.
* The widened capture showing `fread`'s return address.
