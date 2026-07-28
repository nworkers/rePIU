# 20260728-341 작업 로그: quarantine을 거는 세 지점 / Work log: The three quarantine sites

## 한국어

### 결론 요약

**80.24%를 막던 quarantine의 정체가 확정됐습니다. 게임이 자기 자신의 `out` 명령을
1~2바이트 덮어쓰는 세 지점입니다.** 각 쓰기가 4KB 페이지 전체를 영구 격리합니다.

| # | 격리된 페이지 | 쓰기 주체 | 쓰기 대상 | 바이트 | 그 주소의 명령 |
|---|---|---|---|---:|---|
| 1 | `0x030F5000` | `0x030F5CC8` | `0x030F5CC8` | 1 | `out dx, al` |
| 2 | `0x03033000` | `0x030334C6` | `0x030334C6` | 2 | `out dx, ax` |
| 3 | `0x03034000` | `0x03034175` | `0x03034175` | 2 | `out dx, ax` |

**세 경우 모두 쓰기 주체와 대상이 같은 주소이고, 바이트 수가 그 명령의 길이와
같습니다.** 즉 명령 자체를 통째로 덮어쓰는 자기수정입니다. DOS 시절 드라이버가 하드웨어
탐지 후 `out`을 무력화할 때 쓰던 관용구와 일치합니다.

**확인됨: 쓰기 주체가 불명(`source == 0`)이라 기본 격리된 경우는 0건입니다.** 정책의
보수적 기본값이 원인이 아니라 실제 자기수정이 원인입니다.

### 왜 이것이 그렇게 비싼가

`0x030F5000`은 이 실행에서 가장 뜨거운 코드 페이지입니다. baseline 실행의 live
telemetry가 보고한 EIP가 `0x030F508D`, `0x030F5098`로 모두 이 페이지 안입니다.

**한 번의 1바이트 쓰기가 4KB 페이지를 영구히 번역 대상에서 제외합니다.** 그 결과 그
페이지의 모든 실행이 TF walk가 되고, 그것이 Task 340이 측정한 복귀 거절 80.24%,
Task 337이 측정한 5~8개 구간(step의 54%)과 33+ 꼬리(27%)의 실체입니다.

quarantine 이벤트는 60초에 **3회**뿐입니다(요약의 `quarantines: 4` 중 나머지 1건은
동적 번역 실패 경로). **드물게 일어나는 3번의 사건이 실행 시간의 큰 부분을
결정합니다.**

### 처방 후보와 제약

* **(a) 범위 제외 재번역.** 페이지를 통째로 격리하는 대신 패치된 명령만
  `AotExcludedGuestRange`로 빼고 페이지를 재번역한다. 그 기계장치는 이미 있고
  coherence probe가 검증한다. 쓰기가 60초에 3회뿐이므로 재번역 비용은 무시할 수준이다.
* **(b) 바이트 검증 복귀.** quarantine 페이지라도 바이트가 번역 시점과 같다면 복귀를
  허용한다(native-span 음성 캐시와 같은 byte-validated 방식).

**제약:** quarantine은 캐시가 옛 바이트를 실행하는 것을 막으려고 존재합니다. 어느
방식이든 **패치된 바이트가 다시 실행되지 않음**을 보장해야 하며, 그 계약을 설계
문서로 먼저 고정해야 합니다.

**주의:** 패치 대상이 `out`이라는 점이 중요합니다. `out`은 HLE 경계이므로 캐시에서
이미 특별 취급되며, 패치 후 바이트가 무엇이 되는지(예: `nop`)에 따라 경계 분류가
달라질 수 있습니다. 이번 작업에서는 패치 후 값을 읽지 않았습니다.

### 측정 값

같은 60초 Release baseline 실행: progress 129,531, `grBufferSwap` 2,115,
복귀 funnel `segment-write 15,471 / quarantined 153,339 / success 21,561`
(총 190,371), 정상 timeout, malformed 0, fatal 0, Glide 공백 0.

### 확인됨 / Confirmed

* quarantine 3건은 모두 **자기 명령을 덮어쓰는 1~2바이트 쓰기**이며 대상은 전부
  port I/O 명령(`out`)입니다.
* 쓰기 주체 불명으로 인한 기본 격리는 0건입니다.
* 격리된 페이지 중 하나(`0x030F5000`)는 실행이 가장 집중되는 페이지입니다.

### 미확정 / Unresolved

* 패치 **이후의 바이트 값**을 읽지 않았습니다. 재번역 설계에는 그 값이 필요합니다.
* 4번째 quarantine(동적 번역 실패 경로)의 대상 페이지.
* 세 지점이 게임의 어떤 초기화 코드인지(사운드/EEPROM 탐지로 추정하나 미확인).

---

## English

### Summary

The quarantine blocking 80.24% of post-HLE returns is now identified: three sites where the game
overwrites one or two bytes of its own `out` instruction, each permanently quarantining a whole 4KB
page. At `0x030F5CC8` a one-byte write lands on `out dx, al`, at `0x030334C6` a two-byte write on
`out dx, ax`, and at `0x03034175` likewise — in every case the write source equals the destination
and the byte count equals that instruction's length, so the instruction is being overwritten
whole, which matches the DOS-era idiom of disabling a port access after hardware detection. No
event was quarantined for having an unknown write source, so the policy's conservative default is
not the cause; real self-modification is.

### Why it costs so much

`0x030F5000` is the hottest code page in the run — the baseline's live telemetry reports EIPs of
`0x030F508D` and `0x030F5098`, both inside it. One one-byte write excludes that whole page from
translation forever, so everything on it executes as a TF walk. That is Task 340's 80.24% rejection
and Task 337's five-to-eight-step mode and long tail. Only three such events occur in 60 seconds:
three rare events determine a large share of execution time.

### Candidate remedies and the constraint

Either re-translate the page with only the patched instruction excluded, using the existing
`AotExcludedGuestRange` mechanism the coherence probe validates — the write happens three times in
60 seconds, so re-translation cost is negligible — or allow return into a quarantined page once the
bytes are verified unchanged since translation, the byte-validated approach the native-span
negative cache uses. The constraint is that quarantine exists to stop the cache executing stale
bytes, so either approach must guarantee the patched bytes are never executed from the cache, and
that contract belongs in a design document first. It also matters that the patched instruction is
an `out`: port I/O is already an HLE boundary, so what the bytes become after the patch may change
the boundary classification, and this task did not read the post-patch value.

### Measurements and verification

The same 60-second Release baseline reached progress 129,531 with 2,115 buffer swaps and a return
funnel of 15,471 segment-write, 153,339 quarantined, and 21,561 successful out of 190,371, with a
normal timeout, zero malformed dispatch, no fatal halt, and no Glide gap.

### Unresolved

The post-patch byte values were not read and a re-translation design needs them; the fourth
quarantine, from the dynamic-translation-failure path, was not traced; and which initialization
code these three sites belong to — presumed sound or EEPROM detection — is unconfirmed.
