# 작업 지시: Linux x64 릴리스 CI의 간헐적 `host_thread_interrupt=false` (issue #12)

관련: issue #12, v0.0.214 실행 38041054474, `src/tools/aot_probe/host_thread_probe.cpp`

1. `host_thread_interrupt`를 이루는 주장(응답, 콜백 한 번, 0 아닌 표본, 대상 스레드에서 실행, 표본이
   움직임, 편집 도달, 종료)을 따로 기록하고 한 줄로 출력한다. CI가 실패하면 어느 주장인지 남는다.
2. "표본이 움직임" 주장을 CPU의 인터럽트 위치 편향(skid)에 견디게 한다: 대상 스레드가 서로 다른
   두 함수(인라인 금지, 반대 방향으로 세어 ICF로 합쳐지지 않음)를 번갈아 돌게 한다.
3. 검증: Linux x64·i386 core probe 반복, 브랜치에서 `release.yml` 수동 실행(publish는 태그에서만).
4. 작업 로그, issue #12 댓글.

완료 기준: 실패 시 주장별 원인이 출력되고, 표본 위치 편향만으로는 실패하지 않는다.

---

# Work order: intermittent `host_thread_interrupt=false` in the Linux x64 release CI (issue #12)

(1) Record each claim behind `host_thread_interrupt` (answered, one callback, a nonzero sample, run on
the target thread, the sample moves, the edit arrives, the thread joins) and print them on one line, so a
CI failure names its claim. (2) Make "the sample moves" robust to the CPU's interrupt-position bias (skid):
the target alternates between two functions (not inlined, counting in opposite directions so ICF cannot
merge them). (3) Verify with repeated Linux x64 and i386 core probes and a manual `release.yml` run on the
branch (publish runs on tags only). (4) Work log and a comment on issue #12. Done when a failure prints its
claim and a biased sample position alone cannot fail it.
