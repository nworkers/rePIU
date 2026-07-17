# 0x035D6B14 손상값 상수성 조사 작업 로그
# Work Log: 0x035D6B14 Corruption-Value Constancy Investigation

## 1. 범위 (Scope)

Task 223이 남긴 "store(`0x03021F41`)~load(`0x03021F71`) 사이 게스트 스택 슬롯
`0x035D6B14`(`[esp+0x154]`)가 비동기로 손상된다"는 가설을 좁히기 위한 후속 조사.
코드 변경 없음 — 진단 구동, 정적 디스어셈블(`repiu_aot_probe`), 코드 검토만 수행.
상세 결론은 `docs/analysis/current-execution-frontier.md`의 Task 224 절에 누적했다.

## 2. 수행한 것 (What Was Done)

1. **trap 백엔드 장기 구동**(480초, sentinel 불필요): resize 154/212에서 타임아웃
   (`child_exit=124`). fault 미도달(~11분+ 추정). 트레이스 요약은 타임아웃(스레드
   강제종료) 시 출력되지 않아 데이터 미확보. → **trap은 이 fault 관측에 실용성 없음.**
2. **동시 스레드/프로세스 전수 검토**: AOT 번역 워커(`AotTranslationWorkerProc`)는
   요청자가 `WaitForSingleObject(INFINITE)`로 블록되어 비동기 아님·캐시만 씀; CD 오디오
   워커는 자기 힙 버퍼만 씀·`CALLBACK_NULL`; supervisor는 `GetThreadContext` 읽기전용.
   → **게스트 스택 임의 쓰기를 하는 동시 writer 없음.**
3. **예외 프레임 기하 분석**: Windows 예외 디스패치는 ESP 아래로 쓰는데 타겟은 ESP 위
   → int3/single-step/fault 프레임 모두 슬롯에 도달 불가.
4. **post-store 지점(0x21F48) sentinel 실험**(aot-dynamic, ~12초): 0x21F48도 최초
   1회만 발화(문제의 호출 미포착) — 구조적 한계가 store 직후에도 동일.
5. **정적 디스어셈블**(`repiu_aot_probe <EXE> 0x01021F41`): 블록 `0x1021F3E`가
   `0x1021F3E`~`0x1021F9A`(첫 `jb`)까지 단일 직선 basic block. store/load가 그 안에
   함께 있고 중간 정적 진입 없음 → store 항상 실행, 사이 쓰기는 다른 슬롯뿐.
6. **clean vs sentinel fault 덤프 비교**: 스택 레이아웃 동일. 내가 처음 본 "4바이트
   밀림"은 fault_esp=load_esp−4(중간 push 1회) 때문 — Task 222 `D=4` 보정과 동일.

## 3. 핵심 발견 (Key Findings)

* **손상값 `0xDD1523B1`은 런 간 불변 상수.** 정상값은 asset마다 다른데(0x0325E1F8,
  0x0325E214 등) 손상값만 모든 독립 구동에서 동일 → heap/host 포인터가 아니라 고정
  상수/결정론적 계산값. 코드·서드파티에 상수로 없고 알려진 fill 패턴도 아님.
* **fault 시점 EDX=`0xDD1523B1`.** (EDI는 슬롯 load 결과) — 이 상수가 EDX 계산에서
  비롯된다는 강한 단서.
* **오직 `[esp+0x154]` 한 dword만 wild**, 인접 슬롯은 전부 esi=0x0325E208와 일관.
  store는 정상값(esi+0xC=0x0325E214)을 썼음이 재확인됨.

## 4. 배제된 가설 (Ruled Out)

* store 조건부 skip → stale read: 단일 직선 블록이라 반증.
* 별도 스레드(AOT 워커/CD 워커)의 스택 쓰기: 코드 검토로 반증.
* supervisor의 게스트 메모리 쓰기: 읽기 전용.
* 예외 디스패치 프레임의 슬롯 덮어쓰기: ESP 아래로만 써서 도달 불가.
* trap 백엔드로 관측: 실용적 예산 내 fault 미도달.

## 5. 다음 단계 (Next Step)

fault 시점 EDX=`0xDD1523B1`의 **출처 역추적**이 핵심. 함수 진입(`0x03021DF8`)부터 이
상수가 처음 등장하는 지점을 좁히는 표적 진단(소수의 키 게스트 주소에서 EDX/관련
레지스터를 캡처하는 방식, ESP 근접 위험 없는 코드주소 프로브 재사용)을 새 work-order로
설계할 것. `0xDD1523B1`이 무엇을 인코딩하는지(예: asset 헤더/크기/해시, 특정 상수)를
식별하면 근인에 빠르게 접근할 수 있다.

---

**English summary.** Follow-up to Task 223's "async corruption between store and load"
hypothesis; no code change, only diagnostics, static disassembly, and code review. Ruled
out: skipped-store/stale-read (the store/load are in one straight-line basic block
`0x1021F3E`–`0x1021F9A`, so the store always runs), all concurrent writers (AOT worker is
synchronous and cache-only, CD worker writes only its own buffers, supervisor is
read-only), exception-frame overwrite (frames go below ESP; the slot is above ESP), and the
trap backend as an observation tool (480 s reached only resize 154/212, no trace on
timeout). Key findings: the corruption value `0xDD1523B1` is invariant across every
independent run (so a fixed constant / deterministic computation, not a heap/ASLR pointer),
and it also sits in **EDX at fault** — the sharpest remaining lead. The neighbors are all
consistent with a valid esi, confirming the store wrote the correct `0x0325E214` and only
this one dword later became wild. Next step: trace where the constant `0xDD1523B1` in EDX
originates, from function entry `0x03021DF8`, via a new targeted register-capture diagnostic
(reusing the ESP-proximity-free code-address probe). Details accumulate in
`docs/analysis/current-execution-frontier.md` (Task 224 section).
