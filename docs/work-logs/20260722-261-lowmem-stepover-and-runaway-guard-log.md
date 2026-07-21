# 작업 로그 — 저지대 읽기 에뮬레이션이 게스트 코드를 파괴하던 결함 수정: 첫 로고 화면 렌더 / Work Log — Low-Memory Read Emulation Was Destroying Guest Code

* 작성일 / Date: 2026-07-22 (Task 261)
* 브랜치 / Branch: `claude/bga-asset-path-trace`
* 선행 / Predecessor: Task 260 (자산 경로 추적), Task 229 (strtok/stricmp frontier 특성화)

## 1. 결과 / Result

**ANDAMIRO ENTERTAINMENT 로고 화면이 완전히 렌더된다** (사용자 육안 확인).

| 지표 | 수정 전 | 수정 후 |
|---|---:|---:|
| 비검정 픽셀 | 0 | **76,899 / 307,200** |
| 평균 RGB | — | **244,147,2** (로고 주황) |
| 로드된 텍스처 | 1 | **4** (`ARGB_4444` 256×256 ×3 포함) |
| 저지대 읽기 발동 | **1** | **40** |
| 240초 구동 | 조기 종료 | **완주, 미처리 예외 0** |

## 2. 서로를 가리고 있던 결함 2건 / Two Defects That Masked Each Other

### 결함 A — 에뮬레이션이 게스트 명령을 영구 NOP으로 대체

`HandleGuestLowMemoryReadFault`는 널 읽기를 올바르게 에뮬레이트한 뒤,
**그 게스트 명령을 NOP으로 덮어써서** 실행을 재개시켰다.

```cpp
WriteRegisterFromZydis(...);                       // 읽기 에뮬레이트 (정상)
std::vector<std::uint8_t> nop_buffer(len, 0x90);
WriteGuestBytes(context, decode_eip, ...);         // 명령 영구 파괴
```

관측된 유일한 발동 지점은 `0x030F4A98`의 `mov al,[ebx]`(`8A 03`, 2바이트)이며,
Task 229가 이미 **공용 `stricmp(EAX, EDX)` 내부**로 특정해 둔 명령이다. 한 번의
널 문자열 비교를 처리하려고 **이후 모든 `stricmp` 호출의 바이트 로드를 제거**한
셈이다. 파일 확장자 판정(`tga`/`pcx`/`ptx`/`rgb`)이 전부 이 함수를 쓴다.

**결정적 증거:** 수정 전 발동이 **정확히 1회**였다. 빈 텍스처 슬롯마다 반복
발동해야 정상인데, 첫 발동에서 명령이 사라져 이후 fault 자체가 나지 않았다.

**수정:** 코드 패치 대신 **EIP를 명령 길이만큼 전진**시킨다. 디코드 기준도
`decode_eip`(게스트 주소)에서 **실제 실행 주소**로 바꿨다 — fault 시 EIP는 faulting
명령을 가리키므로 이것이 곧 건너뛸 대상이고, AOT에서 두 주소가 다를 때 실행 중인
코드를 기술하는 것도 이쪽이다.

### 결함 B — runaway 가드가 정당한 반복을 폭주로 오판

A를 고치자 즉시 드러났다. 가드는 "같은 EIP·같은 주소 + 50ms 내 5회"를 폭주로 보고
`false`를 반환했고, 그 경로는 fallback 없이 미처리 예외로 종료된다.

그런데 `stricmp(NULL, ...)`은 파일명 하나를 판정할 때
`"tga"`→`"pcx"`→`"ptx"`→`"rgb"` **4연속 호출**되므로 같은 EIP·주소 반복이
**설계상 정상**이다. 75초 시점에 구동이 종료됐다.

**A가 B를 숨기고 있었다.** NOP 패치는 첫 발동에서 명령을 지워 재발동이 없었으므로
B는 **도달 불가능한 코드**였다.

**수정:** EIP 스텝오버는 전진을 구조적으로 보장하므로(우리가 게스트를 그 명령에
붙잡아둘 방법이 없다) 시간 기반 트립을 제거하고 높은 절대 상한만 병리 대비로 남겼다.

## 3. 오판 3건 / Three Misjudgments

전부 기록한다. 같은 유형이 반복됐다.

1. **"저지대 에뮬레이션은 구현·동작 중이므로 원인 아님"** — 크래시가 없다는 것과
   올바르게 동작한다는 것을 혼동했다. 오히려 이 "수정"이 크래시를 **조용한 데이터
   손상**으로 바꿔 추적을 어렵게 만들고 있었다.
2. **"게임이 멈췄다"** — 텔레메트리 끝값만 보고 동결로 단정했다. 실제로는 마지막
   3샘플에서 progress가 73,403 → 78,432 → 84,967로 **증가 중**이었고 로그가 끊긴
   것은 프로세스 종료 때문이었다.
3. **"수정이 배경 문제를 풀지 못했다"** — `andamiro.tga`가 DOS 열기 추적에 없다는
   이유로 실패 판정했다. 그러나 **게임은 아카이브를 메모리에서 조회하므로 DOS
   open을 호출하지 않는다** — 내가 Task 260에서 직접 확인한 사실이었는데도 잘못된
   지표를 1차 기준으로 삼았다. 실제로는 로고가 렌더되고 있었다.

교훈: 지표를 정하기 전에 **그 지표가 인과 사슬의 어디에 있는지** 확인해야 한다.
DOS open은 아카이브 조회 계층 아래에 없다.

## 4. 남은 미확정 / Open

* **파일 열기 109 → 30건 감소.** progress는 비슷한데 열기가 1/3이다. 게임이 다른
  경로를 타는 것으로 보이나 개선/악화 판정은 불가.
* **PTX 디코드 경로.** 텍스처가 1 → 4개로 늘었으나 PTX 엔트리는 465개다.
* **`.PTX` 픽셀 포맷** (Task 260에서 미확정).
* **저지대 0번지 반환값.** `InitializeDosLowMemory`가 전부 0으로 채우므로 실제
  DOS의 인터럽트 벡터 테이블과 다르다. 현재 증상은 없으나 정확성 항목.

## 5. 검증 / Verification

Win32 x86 Debug 빌드 성공. `aot-dynamic pumpit1` 240초 **키 입력 없이** 구동:
완주, 미처리 예외 0, 비검정 픽셀 76,899(avg-rgb 244,147,2)이 swap #2부터 안정
유지, 텍스처 4개 로드. 사용자가 창에서 로고 렌더를 육안 확인.

---

## English Summary

The ANDAMIRO logo screen now renders — 76,899 non-black pixels averaging RGB
(244,147,2), the logo's orange, stable from swap #2, with four textures loaded
against one before.

Two defects were masking each other. `HandleGuestLowMemoryReadFault` emulated the
null read correctly but resumed by **overwriting the guest instruction with
NOPs**. The only site is `mov al,[ebx]` inside a *shared* `stricmp`, so one null
comparison permanently removed the byte load for every later call — and every
filename-extension check (`tga`/`pcx`/`ptx`/`rgb`) goes through that function. The
tell was that it fired exactly **once**: it should recur per empty texture slot,
but the first hit destroyed the instruction. Fixed by stepping EIP over the load
instead of patching, and by decoding at the executing address rather than the
guest address it maps back to.

Fixing that immediately exposed a second defect: the runaway guard treated "same
EIP and address, 5 times within 50 ms" as a runaway and returned false into a
path with no fallback. But `stricmp(NULL, ...)` is called once per candidate
extension, so that repetition is by design; the run died at 75 s. The guard had
been unreachable while the NOP patch existed. Since stepping EIP makes forward
progress structural, the time-based trip was replaced with a high absolute cap.

Three misjudgments are recorded: treating "no crash" as "correct" for the
emulation, reading a truncated telemetry tail as a stall when progress was still
climbing, and declaring the fix a failure because `andamiro.tga` never appears in
the DOS open trace — the game resolves archive entries in memory and never issues
that open, a fact established in Task 260 and then ignored when choosing a
success metric.
