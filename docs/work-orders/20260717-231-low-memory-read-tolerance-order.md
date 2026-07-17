# 작업 지시: DOS/4GW 저지대 메모리 read 관용 구현
# Work Order: Implement DOS/4GW low-memory read tolerance

관련 설계: `docs/design/20260717-231-low-memory-read-tolerance.md`
관련 frontier: `docs/analysis/current-execution-frontier.md` Task 230 절
관련 메모리: task229-bga-texture-extension-frontier

> **상태: 계획만 작성됨(코드 미구현).** 사용자 지시로 이번에는 계획까지만 커밋한다.
> 구현은 후속 작업에서 이 지시서를 따라 진행한다.

## 1. 목표 / Goal

게스트가 DOS/4GW처럼 저지대(널 근처, `< 0x10000`) 메모리를 무해하게 **읽을** 수 있게
하여, `0x030F4A98` read-of-0 크래시를 제거하고 실행을 전진시킨다.

## 2. 변경 대상 / Files

* `src/platform/win32/execution_trampoline.cpp`
  - `HandleDosMemoryAccess`(또는 신설 `HandleGuestLowMemoryReadFault`)를 Zydis 기반
    일반화. VEH AV 경로(현재 `execution_trampoline.cpp:10719` 부근) 트리거를 fault VA
    기반으로 정비.
  - Phase 1: 순수 load(`mov`/`movzx`/`movsx reg,[mem]`) 에뮬레이트(`mov al,[ebx]`=`8A 03`
    포함). 값은 `DosLowMemory` 모델에서 읽고 미초기화는 0. opcode 규약대로 목적 레지스터
    설정(zero/sign 확장) 후 `EIP += 명령 길이`.
  - read 전용 가드, 동일 EIP 반복 fault 카운터, 계측 갱신(`RecordLowMemoryAccess` 재활용
    또는 확장).
* `include/repiu/platform/win32/execution_trampoline.h`
  - 필요 시 계측 필드 추가(저지대 read 에뮬레이트 관련).
* `src/host/win32/main.cpp`
  - 저지대 read 에뮬레이트 계측을 리포트에 노출(감사용).
* `docs/kb/dos4gw-low-memory-model.md`(신규) + `docs/kb/README.md` 색인 갱신.

## 3. 구현 단계 / Steps

1. VEH AV 경로에서 read(`info[0]==0`) && fault VA `< 0x10000` && EIP가 guest/AOT 코드인
   경우로 진입 조건 정비.
2. Zydis로 faulting 명령 디코드(길이·오퍼랜드).
3. Phase 1 load 에뮬레이션 구현(값 출처 = `DosLowMemory`).
4. 안전장치(read 전용, runaway 카운터, 계측) 추가.
5. 빌드(`build/win32_x86_dpmi`, loader).
6. `aot-dynamic pumpit1` 구동으로 `0x030F4A98` 크래시 소멸·전진 확인(가설 확정).
7. trap 백엔드 단시간 회귀 확인.
8. kb 문서 작성, analysis/메모리 갱신, 작업 로그 작성.

## 4. 완료 기준 / Done criteria

* `0x030F4A98`/VA 0 read 크래시 소멸, 실행이 새 frontier로 전진(회귀 없음).
* 저지대 read 에뮬레이트 계측이 기록되고 리포트에 노출.
* Phase 2(ALU read 형태)는 후속 확장 대상으로 명시(당장 미구현 가능).
* 문서(kb/analysis/work-log) 갱신.

## 5. 범위 밖 / Out of scope (이번 계획 커밋 기준)

* 실제 코드 구현(별도 후속 작업).
* write fault 관용(설계상 read 전용 유지).
* Phase 2 ALU read 형태 에뮬레이션(후속).

---

**English summary.** Plan (no code yet, per user instruction to commit only the plan): generalize
the VEH low-memory read-fault emulation so the guest can harmlessly read near-null (`< 0x10000`)
memory as on DOS/4GW, removing the `0x030F4A98` read-of-0 crash. Edit
`execution_trampoline.cpp` to gate `HandleDosMemoryAccess` on a read AV with fault VA `< 0x10000`
from guest/AOT code, Zydis-decode the instruction, and (Phase 1) emulate pure loads
(`mov/movzx/movsx reg,[mem]`, incl. `mov al,[ebx]`) by reading the value from the `DosLowMemory`
model and advancing EIP; add read-only guard, a runaway counter, and telemetry (exposed in
`main.cpp`). Add a `docs/kb/dos4gw-low-memory-model.md` note. Verify via an aot-dynamic `pumpit1`
run (crash gone, execution advances → confirms the Task 230 hypothesis) plus a trap regression.
Phase 2 (ALU-read forms) and write tolerance are out of scope.
