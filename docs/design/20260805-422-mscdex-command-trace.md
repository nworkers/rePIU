# Task 422 설계 — MSCDEX 명령 trace

**한 줄:** Task 421의 위치 census가 **재생 없이 초당 약 65회 상태 변경 명령**이 오가는
구간을 잡아냈는데, 지금 계측은 "마지막 명령" 한 칸뿐이라 **그 폭주가 무슨 명령인지
알 수 없습니다.** 순서를 남깁니다.

## 1. 무엇을 보고 만드는가 (Task 421 census, 사용자 실행)

| 시각 | 관측 |
|---|---|
| 3.5~7.7초 | 곡 프리뷰 재생. `delta_lba` 7~9로 **정상**, 역행 0 |
| **8.0~8.7초** | **`playing=0`인데 generation이 100 ms당 6~7 상승** |
| 8.8초 | 선택곡(LBA 274459) Play 직후 즉시 Stop |
| ~11.2초 | `progress` 동결 후 종료, `last_eip=0x030F2786` |

`generation`은 `Play`·`Stop`·`Seek`에서만 오릅니다. 즉 게스트가 **재생을 만들지 못하는
명령을 반복**하고 있고, 그 직후 실행이 멈춥니다. **정지 EIP는 Task 420의 pumpit2 정지와
같은 주소**이므로, 이 폭주는 frontier 항목 1′의 유력한 선행 사건입니다.

## 2. 왜 지금 계측으로는 못 가리는가

`mscdex_last_command`, `mscdex_last_ioctl_subfunction`처럼 **마지막 값**만 있습니다.
초당 65회가 같은 명령인지 서로 다른 명령의 순환인지, 우리가 성공을 돌려주는데도
반복하는지 실패를 돌려줘서 반복하는지 **구분할 수 없습니다.**

## 3. 설계

명령이 처리된 **뒤** 링(8,192)에 한 줄씩 남깁니다. 요청만이 아니라 **우리가 준 답**을
함께 적는 것이 핵심입니다.

| 필드 | 왜 |
|---|---|
| `wall_ms` | census와 **같은 기준**(실행 시작 상대 ms)이라 두 파일을 겹쳐 읽습니다 |
| `command` · 이름 | `0x03/0x0C` IOCTL, `0x83` seek, `0x84` play, `0x85` stop, `0x88` resume |
| `ioctl_subfunction` | IOCTL이면 어느 서브펑션인지(12 = Q-channel) |
| `address_mode` · `argument_lba` · `argument_length` | seek/play 인자를 **논리 LBA로 변환해** census와 같은 단위로 |
| `success` | **우리가 돌려준 답** — 반복의 원인이 거절인지 아닌지 |
| `current_lba` | 그 순간 게임이 읽었을 위치 |

기록 지점은 `HandleMscdexRequest`의 switch 뒤 한 곳이며, `REPIU_MSCDEX_COMMAND_TRACE=1`
일 때만 링을 만듭니다. **동작은 바꾸지 않습니다.**

**링이 차면 덮어쓰지 않고 카운트만 늘립니다.** 관심 구간은 정지 **직전**이지만, 순서
자체가 자료이므로 앞을 잃는 대신 뒤를 버리고 `overflow`로 알립니다. 8,192건이면
관측된 폭주율(초당 65회)로 **2분 이상**을 담습니다.

## 4. 판정 — 측정 전에 고정

| 관측 | 결론 |
|---|---|
| 같은 `command`+`subfunction`이 `success=1`로 반복 | 게스트가 **우리 답의 내용**에 만족하지 못함 → 그 응답 필드를 봐야 함 |
| 같은 명령이 `success=0`으로 반복 | 우리가 거절 중 → 해당 핸들러의 미구현·조건 실패 |
| `play`가 반복되는데 census의 `playing`이 0 | `Play()`의 조기 반환(트랙 아님·길이 0) |
| `seek` 반복 후 `play` 1회 → 즉시 `stop` | 게스트 상태 기계가 **재생 시작을 확인하지 못함** |

## 5. 하지 않을 것

**아직 고치지 않습니다.** Task 421 §6과 같은 이유이며, 이 trace가 지목하기 전에는
어떤 응답이 문제인지 알 수 없습니다.

---

# Task 422 Design — MSCDEX command trace

**One line:** Task 421's position census caught a window of roughly **sixty-five state-changing
commands per second with nothing playing**, and the existing telemetry keeps only the *last*
command, so **what that storm consists of is unknown**. This records the sequence.

## 1. What prompted it

In the user's run, previews played correctly (`delta_lba` 7-9, zero regressions) until 7.7 s;
then from **8.0 to 8.7 s** `generation` climbed six or seven per 100 ms with `playing` at zero;
at 8.8 s the selected song at LBA 274459 was played and immediately stopped; and by 11.2 s
progress had frozen and the run ended at `last_eip=0x030F2786` — **the same guest address as
Task 420's pumpit2 stall**. Since `generation` only advances on `Play`, `Stop` and `Seek`, the
guest is repeating commands that never produce playback, right before it stalls.

## 2. Why today's telemetry cannot resolve it

Only last-value fields exist. They cannot say whether the sixty-five calls are one command or a
cycle of several, nor whether the guest repeats because we answered **failure** or because it
was unsatisfied with a **successful** answer.

## 3. Design

One line per command, recorded **after** it is served, into an 8,192-entry ring: the wall time
on the **same base as the census** so the two files overlay, the command byte and name, the
IOCTL subfunction, seek and play arguments **converted to a logical LBA** so the units match
the census, **the success we returned**, and the position the game would have read at that
moment. It is written at a single point after the dispatcher's switch, allocated only under
`REPIU_MSCDEX_COMMAND_TRACE=1`, and changes no behaviour. A full ring counts overflow rather
than wrapping, since the ordering is the evidence; 8,192 entries hold over two minutes at the
observed rate.

## 4. Pre-registered readings

The same command repeating with `success=1` means the guest is unsatisfied with the **content**
of our answer, pointing at that response's fields. Repeating with `success=0` means we are
refusing it, pointing at that handler. Repeated `play` while the census shows `playing` at zero
points at `Play()`'s early return. And a run of seeks followed by one play and an immediate
stop means the guest's state machine never confirms that playback started.

## 5. Not yet fixing

For Task 421's reason: until the trace names the response at fault, any fix would be a guess.
