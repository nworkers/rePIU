# Task 743: Linux x64 미번역 return 대상 작업 지시

설계: [20260926-743](../design/20260926-743-x64-untranslatable-return-target.md)

## 한국어

1. `REPIU_AOT_DYNAMIC_REJECT`(주소 또는 `read`) 재현 스위치를 worker의 동적 append 경로에 넣는다.
2. `NoteVehExitSite`가 platform 전역에 마지막 exit site·EIP를 쓰고 fault 보고가 그것을 찍게 한다.
3. `LinuxX64EngineResolver`의 legacy fallback 조건을 읽을 수 있는 guest 코드로 넓힌다.
4. 남는 거절 지점 두 곳에 `[repiu-x64-untranslatable]` 진단을 넣는다.
5. `REJECT=read`로 죽는 지점이 물러나는지, 스위치 없이 attract·플레이·pumpit2a가 그대로인지, core
   probe(Linux·Win32)를 확인한다.
6. 설계·작업 로그·analysis를 갱신하고 커밋한다.

## English

1. Add the `REPIU_AOT_DYNAMIC_REJECT` (address or `read`) reproduction switch to the worker's dynamic
   append path.
2. Have `NoteVehExitSite` publish the last exit site and EIP to platform globals printed by the fault
   report.
3. Widen `LinuxX64EngineResolver`'s legacy fallback to any readable guest target.
4. Add the `[repiu-x64-untranslatable]` diagnostic at the two remaining refusal points.
5. Check that `REJECT=read` dies later, that attract, play and pumpit2a are unchanged without it, and
   that the core probes (Linux and Win32) pass.
6. Update the design, work log and analysis, then commit.
