# Task 711 작업 로그 — PIU.BIN 읽기 루프의 근인 추적

설계: [20260918-711](../design/20260918-711-piu-bin-loop-ss-override.md) ·
작업 지시: [20260918-711](../work-orders/20260918-711-piu-bin-loop-ss-override.md) ·
frontier: [linux-port-frontier](../analysis/linux-port-frontier.md) ·
선행: [20260918-710](20260918-710-linux-x64-timer-safe-points.md)

## 요약

**PIU.BIN 루프의 근인을 찾았고 실험으로 확인했습니다. 그러나 루프를 고쳐도 검은
화면은 그대로입니다.** 루프는 실재하는 결함이지만 텍스처 생략의 원인은 아닙니다.

## 추적

주소는 Linux 실행 기준(image base `0x01010000`, object 2)입니다. Win32에서는
`0x03000000`을 더합니다.

### 1. read는 C 런타임 안에서 올바르게 끝난다

| 주소 | 함수 | 0바이트 read에서 하는 일 |
|---|---|---|
| `0x010F4BD8` | `read` wrapper | `mov ah,3Fh; int 21h` |
| `0x010F24B2` | 버퍼 fill | count 0 → `_EOF` 플래그(`orb $0x10,0xc(%ecx)`) |
| `0x010F1B97` | `fread` | fill이 0이면 `je` 로 반환 |

각 `fread` 호출은 제대로 끝난다. 루프는 `fread`를 **반복해서 부르는 게임 코드**에
있다. 8 dword 스택 캡처는 `fread`의 게임 쪽 반환 주소보다 6 dword 모자랐으므로
24 dword로 넓혔고, word 14에서 `0x0103DB6E`가 나왔다. 정적으로 찾은 `fread` 호출자
75곳 중 `0x0103DB69`가 그것이다.

### 2. 게임의 루프는 `filelength`로 경계가 정해진다

```text
0x0103DB28:
    fp    = fopen(...)                       ; call 0x010F101E
    count = filelength(fp->handle) >> 4      ; call 0x010F23BD; shr ebp,4
    for (i = 0; i < count; i++)
        fread(table + i*16, 1, 16, fp)       ; call 0x010F1B97
    fclose(fp)
```

`PIU.BIN`은 560바이트 = 16바이트 레코드 **35개**이고, Win32의 읽기 37회와 맞는다.
Linux에서는 넓힌 캡처의 `fread` 저장 `ebp`(게임의 루프 경계)가
**`0x0010F0FD` = 1,110,269**였고, Linux 30초 읽기 횟수 1,110,359와 맞는다.

### 3. `filelength`는 `lseek` 세 번이다

```text
0x010F23BD: cur = lseek(h, 0, SEEK_CUR)
            end = lseek(h, 0, SEEK_END)
                  lseek(h, cur, SEEK_SET)
            return end
```

### 4. `lseek` wrapper는 결과를 **SS override로** 스택에 쓴다

```text
0x010F44D0: push ecx; push edi; sub esp,4
            mov edi,esp
            ...  mov ah,42h ... int 21h
            mov ss:[edi],ax          ; 66 36 89 07
            mov ss:[edi+2],dx        ; 66 36 89 57 02
            ...
            mov eax,[esp]            ; 결과 = DX:AX
```

우리 `AH=42h` HLE는 올바르다 — `AX`에 하위, `DX`에 상위 16비트, carry 해제.

### 5. 그런데 게스트 SS는 base가 0이 아니다

`REPIU_LINEXE_INIT_TRACE=1`로 확인했다.

```text
[repiu-guest-segments] initial_ds=true selector=0x0024 initial_ss=true selector=0x0034
Win32 relocated selector binding: selector=0x0034 object=4 base=0x01110000
```

게스트 `ESP`(`0x0158CA5C`)는 **linear** 주소인데, SS 선택자 `0x0034`의 base는
`0x01110000`이다. 명시적 `SS:` override가 붙은 메모리 피연산자는 translation plan에서
segment-override로 분류되고(`IsTranslatableSegmentOverrideMem`),
`BuildAotSegmentTable`이 그 선택자의 descriptor base를 fold한다. 그래서
`ss:[edi]`는 `edi + 0x01110000`으로 가고 `[esp]`에는 닿지 않는다. `lseek`은
그 슬롯에 남아 있던 값을 결과로 돌려준다.

Linux에서는 `cur`와 `end`가 **같은** 낡은 값 `0x010F0FDF`였다. 그래서 복원 seek가
거기로 가고, `count = 0x010F0FDF >> 4 = 0x0010F0FD`가 된다.

### 6. 실험 — 커밋하지 않음

`BuildAotSegmentTable`에서 SS의 fold base를 0으로 두는 임시 패치를 환경 변수 뒤에
넣고 같은 바이너리로 두 번 돌렸다.

| Linux x64, 30초 | SS fold = 선택자 base | SS fold = 0 (실험) | Win32 |
|---|---:|---:|---:|
| DOS read count | 1,110,356 | **91** | 122 |
| Glide gate #51 | `_GRALPHACOMBINE@20` | `_GRALPHACOMBINE@20` | `_GRTEXTEXTUREMEMREQUIRED@8` |
| 삼각형 #3 | combine=1/other=2, texEnabled=0, non-black 0 | **같음** | combine=3/other=1, texEnabled=1 |

**루프 근인은 확인됐다.** 그러나 **텍스처 블록은 여전히 생략되고 화면은 여전히
검다.** 실험 패치는 되돌렸다.

## Win32에도 같은 결함이 있다 (추정)

Win32 trace에서 PIU.BIN의 복원 seek도 쓰레기 offset(`0x0458CC60`, 게스트 스택 주소)
으로 간다. 즉 Win32에서도 적어도 한 번은 `SS:` 저장이 `[esp]`에 닿지 않는다. 그 뒤
35번의 `fread`는 전부 0바이트이므로 **Win32에서도 PIU.BIN의 레코드 테이블은 채워지지
않는 것으로 보인다.** Win32의 `end`가 옳게 나온 것은 그 호출의 저장이 native 경로에서
평탄한 host SS로 실행됐기 때문으로 추정한다. 이것은 trace에서 추론한 것이고 직접
확인하지 않았다.

## 코드 변경과 검증

`DosFileIoTraceEntry::guest_stack`을 8 → 24 dword로 넓혔다. 다른 변경은 없다.

* Linux x64 Debug 빌드, core probe **29/29**
* Win32 x86 Debug 전체 빌드 오류 0, core probe **27/27**

## 다음

1. **SS(와 아마 DS/ES) override의 fold 의미.** 게스트는 flat 모델처럼 linear 주소를
   쓰는데, loader는 SS/DS에 object base를 가진 선택자를 준다. 명시적 override가 그
   base를 fold하면 주소가 어긋난다. 이것은 **선택자 모델의 결정**이고 Win32 동작도
   바꾸므로 따로 설계하고 확인받는다.
2. **텍스처 블록 생략의 진짜 원인.** 루프를 고쳐도 gate #51은 그대로다. 다른 원인을
   다시 두 host 비교로 찾는다.

---

## English

Design: [20260918-711](../design/20260918-711-piu-bin-loop-ss-override.md) ·
Work order: [20260918-711](../work-orders/20260918-711-piu-bin-loop-ss-override.md) ·
Frontier: [linux-port-frontier](../analysis/linux-port-frontier.md) ·
Predecessor: [20260918-710](20260918-710-linux-x64-timer-safe-points.md)

### Summary

**The cause of the PIU.BIN loop was found and confirmed by experiment. Fixing the
loop does not fix the black screen.** The loop is a real defect, but it is not
why the texture upload is skipped.

### Trace

Addresses are from a Linux run (image base `0x01010000`, object 2); add
`0x03000000` for Win32.

1. **The read ends correctly inside the C runtime.** The read wrapper at
   `0x010F4BD8` issues `mov ah,3Fh; int 21h`; the buffer fill at `0x010F24B2`
   sets `_EOF` on a zero count; `fread` at `0x010F1B97` returns on a zero fill.
   The loop is in the **game code that keeps calling `fread`**. The 8-dword
   capture stopped six dwords short of `fread`'s return into the game, so it was
   widened to 24; word 14 gave `0x0103DB6E`, i.e. the call at `0x0103DB69` among
   the 75 static `fread` callers.
2. **The game loop is bounded by `filelength`.** At `0x0103DB28` the game does
   `count = filelength(fp->handle) >> 4` and then
   `for (i = 0; i < count; i++) fread(table + i*16, 1, 16, fp)`. `PIU.BIN` is 560
   bytes, **35 records**, matching Win32's 37 reads. On Linux, `fread`'s saved
   `ebp` — the game's loop bound — was **`0x0010F0FD` = 1,110,269**, matching the
   1,110,359 reads.
3. **`filelength` is three `lseek` calls**: `cur = lseek(h,0,SEEK_CUR)`,
   `end = lseek(h,0,SEEK_END)`, `lseek(h,cur,SEEK_SET)`, returning `end`.
4. **The `lseek` wrapper at `0x010F44D0` stores its result through an `SS:`
   override** — `mov ss:[edi],ax` and `mov ss:[edi+2],dx` with `edi = esp` — and
   then reads `mov eax,[esp]`. Our `AH=42h` HLE is correct: low word in `AX`, high
   word in `DX`, carry clear.
5. **But the guest SS has a nonzero base.** `REPIU_LINEXE_INIT_TRACE=1` shows
   `initial_ss` selector `0x0034`, bound to object 4 at base `0x01110000`, while the
   guest `ESP` (`0x0158CA5C`) is linear. An explicit `SS:` memory operand is
   classified as a segment override (`IsTranslatableSegmentOverrideMem`) and
   `BuildAotSegmentTable` folds that selector's descriptor base, so `ss:[edi]`
   goes to `edi + 0x01110000` and never reaches `[esp]`; `lseek` returns whatever
   was already in the slot. On Linux `cur` and `end` were the **same** stale value
   `0x010F0FDF`, so the restore seek went there and
   `count = 0x010F0FDF >> 4 = 0x0010F0FD`.
6. **Experiment, not committed.** A temporary patch behind an environment
   variable folded SS with base 0; the same binary ran both ways for 30 seconds.
   Reads fell from **1,110,356 to 91** (Win32: 122). Gate #51 stayed
   `_GRALPHACOMBINE@20`, and triangle #3 stayed `combine=1/other=2`,
   `texEnabled=0`, 0 non-black pixels. **The loop's cause is confirmed; the texture
   block is still skipped and the screen is still black.** The patch was reverted.

### Win32 has the same defect (inferred)

In the Win32 trace, the restore seek on PIU.BIN also goes to a garbage offset
(`0x0458CC60`, a guest stack address), so on Win32 too at least one `SS:` store
misses `[esp]`. The 35 `fread`s that follow all return 0 bytes, so **PIU.BIN's
record table appears never to be filled on Win32 either.** Win32's `end` came
out right presumably because that call's stores ran on the native path under the
host's flat SS. This is inferred from the trace and was not measured directly.

### Code change and verification

`DosFileIoTraceEntry::guest_stack` widened from 8 to 24 dwords; nothing else
changed. Linux x64 Debug built and core probe passed **29 of 29**; the Win32 x86
Debug full build had no errors and its core probe passed **27 of 27**.

### Next

1. **What an SS (and probably DS/ES) override should fold.** The guest addresses
   memory linearly like a flat model, while the loader gives SS/DS selectors with
   object bases, so an explicit override folding that base misaddresses. This is a
   **decision about the selector model** that also changes Win32 behavior, so it
   gets its own design and a confirmation.
2. **The real cause of the skipped texture block.** With the loop fixed, gate #51
   is unchanged; find the other cause by comparing the hosts again.
