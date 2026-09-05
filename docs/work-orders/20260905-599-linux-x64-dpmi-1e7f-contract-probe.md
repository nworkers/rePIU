# 작업 지시 20260905-599 — Linux x64 DPMI `1E7Fh` 계약 탐침

## 목표

미확인 DOS4GW 사설 DPMI 호출 `INT 31h AX=1E7Fh`의 성공 경로를 안전하게
관측하되, 기본 HLE 계약이나 원본 guest 코드를 변경하지 않습니다.

## 작업

1. `1E7Fh` 전용 opt-in 입력 레지스터 trace를 추가합니다.
2. 명시적 환경 변수에서만 CF clear·레지스터 보존을 하는 probe-success 경로를
   추가합니다.
3. Linux x64 빌드와 core probe를 실행합니다.
4. 기본, trace, probe-success 실행을 각각 기록합니다.
5. 결과를 `linux-port-frontier.md`와 작업 로그에 확인됨/추정/미확정으로
   구분하여 기록합니다.

## 완료 기준

* 기본 실행 의미가 Task 595와 동일합니다.
* trace가 `1E7Fh` 입력 전체를 출력합니다.
* probe-success가 오류 분기와 구별되는 다음 frontier 또는 새 blocker를 보입니다.
* Linux x64 build와 core probe가 성공합니다.

---

# Work order 20260905-599 — Linux x64 DPMI `1E7Fh` contract probe

## Goal

Safely observe the success path of the unconfirmed DOS4GW-private DPMI call
`INT 31h AX=1E7Fh` without changing the default HLE contract or original guest
code.

## Work

1. Add opt-in entry-register tracing specific to `1E7Fh`.
2. Add a probe-success path that clears CF and preserves registers only under
   an explicit environment variable.
3. Run the Linux x64 build and core probe.
4. Record default, trace, and probe-success executions separately.
5. Update `linux-port-frontier.md` and the work log with confirmed, inferred,
   and unresolved results clearly separated.

## Done criteria

* Default execution semantics remain identical to Task 595.
* Trace prints complete `1E7Fh` inputs.
* Probe-success shows a distinct next frontier or blocker rather than the
  error branch.
* Linux x64 build and core probe succeed.
