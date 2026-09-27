# Task 748: 페이싱 아래 "잠깐 멈칫" 조사 — 긴 프레임 분해 진단과 스크립트 키보드

## 한국어

### 배경

Task 747 뒤 사용자 보고: "전체적으로 좋아졌는데, 게임 중 노트와 BGA가 잠깐씩 멈칫한다. MP3 재생 위치
업데이트가 잠깐씩 멈추는 것으로 의심된다." 페이싱(`swap_interval = 1`, Task 745)을 켠 Linux 호스트에서의
증상이다. 기존 계측으로는 답할 수 없는 물음이 셋이다.

1. 멈칫이 **프레임 하나가 길어진 것**인지(게스트가 오래 걸렸는지, present가 오래 걸렸는지, 페이서가
   늦게 깼는지), 아니면 프레임은 제때인데 **디스플레이 위상**이 어긋난 것인지.
2. 그 순간 MP3 시계가 정말 멈췄는지 — 20 ms census(`REPIU_PIU10_MP3_CENSUS_MS`)는 있지만 프레임 시각과
   같은 축에 없다.
3. 재현이 결정적인지 — WSLg의 XTest 합성 키는 창 포커스에 따라 곡 선택까지 못 가는 날이 있었다.

### 설계

1. **긴 프레임 로그** `REPIU_GLIDE_LONG_FRAME_LOG=1`. `grBufferSwap`의 present 뒤에서 이전 present 끝부터의
   프레임 길이가 주기의 두 배(페이싱이면 페이싱 주기, 아니면 16.667 ms)를 넘으면 stderr에 한 줄:
   `[repiu-glide-swap] long-frame elapsed_ms= frame= frame_us= sleep_us= guest_us= present_us=`.
   프레임 = 직전 페이서 sleep + 게스트 구간(다음 swap 진입까지) + present 이므로 셋을 따로 찍는다. stderr에
   두는 이유는 MP3 census·자산 트레이스가 같은 스트림에 있어 한 축에서 읽히기 때문이다. 페이싱과 무관하게
   찍히므로 Win32 real vsync에서도 쓸 수 있다.
2. **페이서 지터 카운터**를 최종 보고에 더한다: `late-swaps/late-max-us/resyncs/oversleep>1ms/oversleep-max-us`.
   sleep의 정확도(oversleep)와 마감 재동기 횟수(resyncs = 한 주기 이상 밀린 프레임 수)를 갈라 읽는다.
3. **스크립트 키보드** `REPIU_INPUT_SCRIPT=<파일>`. `<ms> <키 이름> [hold ms]` 줄(`#` 주석)을 창이 열린
   순간부터 재어 SDL 키 이벤트로 밀어 넣는다. 같은 펌프·바인딩·JAMMA 타임라인을 지나므로 게스트는 실제
   키보드와 구별하지 못하고, XTest와 달리 포커스에 의존하지 않는다. `scripts/input_scripts/pumpitea_play.txt`가
   SERVICE 5회 → 시작 → 곡 선택 → 확정까지를 담는다.
4. **자산 트레이스 상한 해제** `REPIU_DOS_ASSET_TRACE=all`. 기존 상한(열기 200·읽기 120·seek 120)은 곡
   중의 읽기를 숨긴다. `all`이면 모두 찍는다(무겁다: ftell 폴링이 초당 수천 줄).

### 검증 전략

페이싱 플레이 60초를 스크립트로 세 번 돌려 곡 구간(36–50 s)의 긴 프레임을 세고, 각 긴 프레임을
guest/present/sleep으로 가른 뒤 같은 시각의 census·자산 트레이스·100 ms 라이브 프로파일과 맞춘다.
페이싱 없는 실행에서도 로그가 찍히는지, core probe(Linux·Win32)가 통과하는지 본다.

## English

### Background

After Task 747 the user reported: "Better overall, but during play the arrows and the BGA hitch
briefly now and then; I suspect the MP3 playback position stops updating for a moment." This is on a
Linux host with pacing on (`swap_interval = 1`, Task 745). Three questions the existing instruments
cannot answer:

1. Is a hitch **one long frame** (the guest, the present, or a late pacer wake-up), or are the frames
   on time and the **display phase** off?
2. Did the MP3 clock stall at that moment? The 20 ms census exists but is not on the frame's time axis.
3. Is the reproduction deterministic? XTest synthetic keys under WSLg depend on window focus and some
   days never reached the song.

### Design

1. **Long-frame log** `REPIU_GLIDE_LONG_FRAME_LOG=1`. After the present in `grBufferSwap`, when the
   frame since the previous present's end exceeds two periods (the pacing period, or 16.667 ms without
   pacing), one stderr line: `[repiu-glide-swap] long-frame elapsed_ms= frame= frame_us= sleep_us=
   guest_us= present_us=`. A frame is the previous pace's sleep, the guest's share up to the next swap
   and the present, so the three are printed apart. It goes to stderr because the MP3 census and the
   asset trace are there, on the same axis. It does not depend on pacing, so it serves Win32's real
   vsync too.
2. **Pacer jitter counters** in the final report: `late-swaps/late-max-us/resyncs/oversleep>1ms/
   oversleep-max-us`, separating the sleep's accuracy from the number of frames more than a period late.
3. **Scripted keyboard** `REPIU_INPUT_SCRIPT=<file>`: `<ms> <key name> [hold ms]` lines (`#` comments),
   timed from the moment the window opened, pushed as SDL key events. They pass the same pump, bindings
   and JAMMA timeline, so the guest cannot tell them from a keyboard, and unlike XTest they do not depend
   on focus. `scripts/input_scripts/pumpitea_play.txt` goes from five SERVICE credits to a confirmed song.
4. **Uncapped asset trace** `REPIU_DOS_ASSET_TRACE=all`: the caps (200 opens, 120 reads, 120 seeks) hide
   whatever a song reads; `all` prints everything (heavy: the ftell polling is thousands of lines a second).

### Verification strategy

Three paced 60 s scripted plays, counting the long frames inside the song (36–50 s), splitting each into
guest/present/sleep and matching them against the census, the asset trace and the 100 ms live profile at
the same instant; the log without pacing; the core probes (Linux, Win32).
