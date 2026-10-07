# 설계: 뜨거운 INT 21h 서비스의 호스트 직접 디스패치 (issue #18)

## 근거가 되는 측정

* 방향 3(1~3단계) 뒤에도 pumpitea에는 두 정지가 남는다: 로고 다음
  공백과 **두 번째 정지**(이 날 측정 47.5~50.0초 구간, 2.5~3.8초).
  두 번째 정지 동안 게스트는 멈춘 것이 아니라 **HLE 디스패치가 초당
  857 → 12,500~19,000회로 폭증**하고 `last_eip`가 lseek 래퍼
  (`0x04101BAE`, AH=42h)다 — 데모 데이터를 수만 번의 seek/read로 읽는
  파일 I/O가 VEH 비용으로 늘어진 것이다(~45,000회 × VEH 왕복).
* 로딩 공백의 VEH 바닥도 같은 구조다: INT 21h AH=2Ch 시간 루프
  (~10만 회/30초)와 AH=42h lseek(~3.5만 회/30초)가 예외 경로로 돈다.
* 기존 `REPIU_AOT_DBT_SUPERBLOCK`(Task 308) 호스트 디스패치는 이미
  발화하지만(90초에 진입 438,434회), **`RequiresVehMediatedHle`가 모든
  INT 명령을 일괄 제외**해 절반(222,309회)이 `veh-required`로
  폴백한다 — on/off 교대 측정에서 공백 차이가 없는 이유다.

## 설계

`RequiresVehMediatedHle`의 INT 일괄 제외에 **INT 21h 화이트리스트
예외**를 더한다. 호출부가 frame의 EAX를 넘겨 AH를 검사한다:

* 허용: `INT 0x21` 이고 AH ∈ {0x2C(시각), 0x3F(read), 0x42(lseek)}.
* 그 외 INT/INT3/INTO/IRET와 far 전송, 세그먼트·ESP 기록 명령은
  기존대로 VEH로 남는다.

세 서비스의 안전 근거 — 디스패처의 합성 컨텍스트 계약과 맞는다:

| 서비스 | 효과 | ESP | 세그먼트 | EIP |
|---|---|---|---|---|
| AH=2Ch | ECX/EDX/CF 기록 | 불변 | 불변 | +2 |
| AH=42h | EAX/EDX/CF, 핸들 상태 | 불변 | 불변 | +2 |
| AH=3Fh | EAX/CF, 게스트 메모리 기록(호스트 측) | 불변 | 불변 | +2 |

디스패처의 기존 후처리가 그대로 적용된다: ESP 변화는 state-mismatch로
잡고, `EIP=원지점+2`는 같은 블록의 연속으로 `ResolveAotTransferTarget`이
해석하며, 실패 시 one-step 다리로 복귀한다. 서비스 구현은 VEH 경로와
동일한 `HandleTracedDosInterrupt21`(→`HandleDosInterrupt21`)이다.

전체 기능은 기존대로 `REPIU_AOT_DBT_SUPERBLOCK` 옵트인 뒤에 있다.
측정이 좋고 회귀가 없으면 기본 켜기(승격)는 Task 386(Port-I/O 승격)의
절차를 따라 별도 판단한다.

## 검증

1. probe: selector guard probe의 superblock HLE 절 통과 유지.
2. pumpitea 90초, `REPIU_AOT_DBT_SUPERBLOCK=1` 대 미설정 교대 ≥2쌍:
   두 공백, `HLE host dispatch entry/success/fallback`과
   `veh-required` 감소, handled DOS 카운터 불변(의미 보존) 확인.
3. pumpit1/pumpit2a/pumpit3a 30초 스모크(toggle on).

---

# Design: host-direct dispatch for hot INT 21h services (issue #18)

After direction 3, two stalls remain in pumpitea: the post-logo gap and
a second stall (2.5–3.8 s around 47–50 s this day) during which HLE
dispatches burst from 857/s to 12.5k–19k/s with `last_eip` at the
AH=42h lseek wrapper — demo-data file I/O stretched by VEH round trips
(~45k calls). The loading gap's VEH floor has the same structure
(AH=2Ch time loops ~100k/30 s, AH=42h ~35k/30 s). The existing
`REPIU_AOT_DBT_SUPERBLOCK` host dispatch already fires (438,434 entries
per 90 s) but `RequiresVehMediatedHle` blanket-rejects every INT, so
half of it (222,309) falls back `veh-required` — which is why the
toggle measured no gap change.

Design: add an INT 21h allowlist to that blanket rejection, with the
caller passing the frame's EAX so AH can be inspected: allow
`INT 0x21` with AH ∈ {0x2C get-time, 0x3F read, 0x42 lseek}; everything
else keeps its VEH mediation. All three services leave ESP and the
segment registers untouched, advance EIP by 2 (resolved in-cache as the
block's own continuation, with the one-step bridge as the miss path),
and run the same `HandleTracedDosInterrupt21` implementation the VEH
uses; the dispatcher's existing ESP state-mismatch guard stays. The
whole mechanism remains behind the `REPIU_AOT_DBT_SUPERBLOCK` opt-in;
promotion to default is a separate decision following the Port-I/O
precedent (Task 386) once measured.

Verification: the probe's superblock section still passes; interleaved
pumpitea runs toggle-on against toggle-off (both gaps, dispatch
success/`veh-required` counters, handled-DOS counters unchanged in
meaning); 30 s smoke on pumpit1/pumpit2a/pumpit3a with the toggle on.
