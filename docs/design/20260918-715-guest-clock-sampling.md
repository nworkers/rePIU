# Task 715 설계 — 게스트 시계를 벽시계와 함께 기록하기

## 목적

Task 714는 Linux x64가 같은 장면을 느리게 진행하고, safe point 틱이 0건이라는 것을
보였다. 그러나 "틱 부족 때문에 게임 시계가 느리다"는 가설은 확인하지 못했다. 주입을
켠 실행이 크래시해서 요약이 남지 않았기 때문이다.

이 작업은 주입을 켜지 않고도 가설을 판정한다. **게스트가 스스로 세는 틱 카운터를
벽시계와 함께 두 host에서 기록한다.**

## 설계

`REPIU_LIVE_GUEST_PEEK=<runtime offset>[,개수]`를 둔다. 매초의 `[repiu-live]` 줄
옆에 다음 줄을 남긴다.

```text
[repiu-live-peek] elapsed_ms=N offset=0x... values=XXXXXXXX ...
```

* offset은 **runtime base 기준**이다. 그래서 한 값이 두 host에서 같은 게스트
  변수를 가리킨다(image base는 Win32 `0x04000000`, Linux `0x01000000`).
* dword는 최대 16개이고, 읽을 수 없는 범위면 줄을 남기지 않는다.
* host 스레드에서 게스트와 동기화 없이 읽는다. 카운터 표본에는 그것으로 충분하다.
* 설정하지 않으면 아무 일도 하지 않는다.

## 무엇을 읽는가

pumpit2a의 INT 8 ISR(object 2 오프셋 `0x2F132`)은 모든 틱에서 전역 카운터 하나를
증가시킨다. 파일 바이트에는 `ff 05 20 fa 17 00`으로 보이지만, 그 원시 값
`0x17FA20`은 **object 4 안의 offset**이다. 재배치된 코드를 실행 중에 읽어
`incl [0x0428FA20]`(Win32)임을 확인했다. 두 host 공통 runtime offset은
`0x28FA20`이다.

처음에는 원시 값을 object 4의 relocation base(`0x120000`) 기준으로 환산해
`0x16FA20`을 읽었고 두 host 모두 30초 내내 0이었다. 그 결과가 환산 오류를 드러냈다.

## 검증

* 두 host core probe
* 두 host 30초 기록 비교, 그리고 Linux에서 Task 714 opt-in 주입을 켠 기록

---

## English

### Purpose

Task 714 showed that Linux x64 progresses through the same scenes more slowly and
injects no ticks at safe points, but could not confirm that tick starvation slows
the guest's clock: runs with injection on crashed before writing a summary. This
task settles the hypothesis without turning injection on, by **recording the
guest's own tick counter against wall time on both hosts**.

### Design

`REPIU_LIVE_GUEST_PEEK=<runtime offset>[,count]` adds a line beside each per-second
`[repiu-live]` line:
`[repiu-live-peek] elapsed_ms=N offset=0x... values=XXXXXXXX ...`. The offset is
**relative to the runtime base**, so one value names the same guest variable on
both hosts (image base `0x04000000` on Win32, `0x01000000` on Linux). Up to 16
dwords are read; an unreadable range writes no line. The read happens on the host
thread without synchronizing with the guest, which is enough for sampling a
counter. Unset, it does nothing.

### What is read

pumpit2a's INT 8 ISR (object-2 offset `0x2F132`) increments one global counter on
every tick. The file bytes show `ff 05 20 fa 17 00`, but the raw `0x17FA20` is an
**offset inside object 4**; reading the relocated code at run time showed
`incl [0x0428FA20]` on Win32. The shared runtime offset is `0x28FA20`. The first
attempt converted the raw value through object 4's relocation base (`0x120000`),
read `0x16FA20`, and saw zero on both hosts for 30 seconds — which is what exposed
the conversion error.

### Verification

* Core probes on both hosts.
* 30-second recordings on both hosts compared, plus a Linux recording with Task
  714's opt-in injection on.
