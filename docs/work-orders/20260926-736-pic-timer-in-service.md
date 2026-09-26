# Task 736: 타이머 IRQ0 in-service와 `sti` 시점 전달 작업 지시

설계: [20260926-736](../design/20260926-736-pic-timer-in-service.md)

## 한국어

1. 사용자 로그(`repiu_log.txt`)에서 정지 전 PIT 설정과 fault stack을 읽고 ISR을 역어셈블한다.
2. `PicTimerInService` 정책(in-service 비트, EOI, stack fallback, `sti` 요청과 연쇄 금지)을
   별도 파일로 만들고 core probe `pic_timer_in_service`를 추가한다.
3. `InjectPendingInterrupts`에 정책을 연결하고, port 0x20 쓰기에서 EOI를 알리며, 에뮬레이트된
   `sti`가 요청을 세우고, 일반 fault 경로의 privileged-trap HLE 뒤에 주입 시도를 둔다.
4. 주입 frame의 EFLAGS에서 TF를 지운다.
5. `REPIU_PIC_TIMER_IN_SERVICE=0` 스위치를 두고 최종 보고에 카운터를 출력한다.
6. Linux x64와 Win32에서 core probe, pumpitea 반복 실행, pumpit2a 회귀를 스위치 켬/끔으로
   비교한다.
7. 설계, 작업 로그, ARCHITECTURE, analysis, kb, `EXE_DESIGN.*`를 갱신하고 커밋한다.

## English

1. Read the PIT setting and the fault stack before the failure in the user's log (`repiu_log.txt`),
   and disassemble the ISR.
2. Put the `PicTimerInService` policy (in-service bit, EOI, stack fallback, `sti` request and no
   chaining) in its own files, and add the `pic_timer_in_service` core probe.
3. Wire the policy into `InjectPendingInterrupts`, report EOIs from port 0x20 writes, have the emulated
   `sti` set the request, and add an injection attempt after the general fault path's privileged-trap
   HLE.
4. Clear TF from the injected frame's EFLAGS.
5. Add the `REPIU_PIC_TIMER_IN_SERVICE=0` switch and report the counters in the final report.
6. Compare the core probe, repeated pumpitea runs and pumpit2a regressions on Linux x64 and Win32 with
   the switch on and off.
7. Update the design, work log, ARCHITECTURE, analysis, kb and `EXE_DESIGN.*`, then commit.
