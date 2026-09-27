# Task 747: x64 HLE 재진입 뒤 tick 주입 작업 지시

설계: [20260927-747](../design/20260927-747-inject-ticks-after-hle-on-x64.md)

## 한국어

1. `HandleAotReentry`의 planner-HLE 분기에서 HLE 처리 뒤 resume 전에 `InjectPendingInterrupts`를
   부른다.
2. 페이싱 attract 14초로 dropped·max-backlog가 내려가는지 재고, 페이싱 60초와 플레이 60초, 페이싱 없는
   실행, pumpit2a, core probe(Linux·Win32)를 확인한다.
3. 설계·작업 로그·analysis를 갱신하고 커밋한다. 사용자에게 노트·노이즈 확인을 요청한다.

## English

1. Call `InjectPendingInterrupts` in `HandleAotReentry`'s planner-HLE branch after the HLE handled
   the instruction and before the resume.
2. Remeasure a paced 14 s attract for dropped and max-backlog, then check a paced 60 s run, a 60 s
   play, unpaced runs, pumpit2a and the core probes (Linux, Win32).
3. Update the design, work log and analysis, commit, and ask the user to confirm arrows and audio.
