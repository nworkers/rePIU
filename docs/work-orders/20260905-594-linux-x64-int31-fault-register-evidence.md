# 작업 지시 20260905-594 — Linux x64 INT 31h fault 레지스터 귀속

설계: [20260905-594](../design/20260905-594-linux-x64-int31-fault-register-evidence.md)

## 작업

1. Linux fault reporter가 `GuestCpuContext` snapshot을 받을 수 있도록
   `ReportUnhandledFault` 시그니처와 호출부를 갱신합니다.
2. 기존 fault 필드를 보존하면서 guest GPR과 `EFLAGS`를 async-signal-safe
   hex 출력으로 추가합니다.
3. Linux x64 빌드와 `repiu_core_probe`로 검증합니다.
4. 가능하면 watched `pumpit2a`를 재현하고, 확인된 레지스터를
   `docs/analysis/linux-port-frontier.md`와 작업 로그에 반영합니다.
5. 변경을 하나의 Git 커밋으로 남깁니다.

## 완료 조건

* 미처리 Linux fault line에 `eax`부터 `eflags`까지 출력됩니다.
* fault 처리 제어 흐름은 기존과 동일합니다.
* 빌드/프로브 결과와 Linux 재현 가능 여부가 작업 로그에 기록됩니다.

---

# Work order 20260905-594 — Linux x64 INT 31h fault register attribution

Design: [20260905-594](../design/20260905-594-linux-x64-int31-fault-register-evidence.md)

## Work

1. Update the Linux fault reporter signature and call site to accept the
   `GuestCpuContext` snapshot.
2. Preserve the existing fault fields and append guest GPRs and `EFLAGS` using
   async-signal-safe hexadecimal output.
3. Verify with the Linux x64 build and `repiu_core_probe`.
4. If possible, reproduce watched `pumpit2a` and record the confirmed registers
   in `docs/analysis/linux-port-frontier.md` and the work log.
5. Leave one Git commit for the change.

## Done when

* An unhandled Linux fault line prints `eax` through `eflags`.
* Fault control flow is unchanged.
* Build/probe results and Linux reproduction availability are recorded in the
  work log.
