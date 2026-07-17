# 손상 블록 제어흐름 컨텍스트 매핑 작업 로그
# Work Log: Corrupting-Block Control-Flow Context Mapping

## 1. 범위 (Scope)

Task 224의 "fault 시점 EDX=손상값(0xDD1523B1)" 리드를 좇아, 손상 슬롯
`0x035D6B14`(`[esp+0x154]`)를 쓰는/읽는 블록 주변의 제어흐름을 완전히 매핑했다.
코드 변경 없음 — `repiu_aot_probe` 정적 디스어셈블과 기존 AOT return/transfer 트레이스
분석만 수행. 상세 결론은 `docs/analysis/current-execution-frontier.md` Task 225 절.

## 2. 수행한 것 (What Was Done)

1. **AOT return trace 분석**(clean aot-dynamic 구동): 함수 0x03021DF8의 일련의 call
   반환이 전부 mispredict(`expected=0x030F5153` 고정, `match=false`), 마지막 반환은
   `actual=0x03021F36, ESP=0x035D69BC`. → Tasks 219-220 return thrashing과 동일.
2. **블록 직전 디스어셈블**(`repiu_aot_probe <EXE> 0x01021F36`): `call 0x030F4330`
   (0x21F31, 반환점 0x21F36) → `test edi,edi`(0x21F36) → `jle 0x030220A8`(0x21F38)
   → fall-through 시 블록 0x21F3E(store). 블록은 EDI>0일 때 fall-through로 진입.
3. **memset 루틴 특성**(`repiu_aot_probe <EXE> 0x010F4330`): `0x030F4330`은 DL 바이트를
   EDX에 4바이트 복제 후 채우기 헬퍼(0x030F5F30) 호출 = memset. 0xDD1523B1은 복제형이
   아니므로 채움값 아님.
4. **mid-block 진입 가설의 혼란 요인 규명**: int3 sentinel 관측은 "문제의 호출이
   store를 건너뛴다"고 시사하나, 블록 내 int3가 AOT 인라인 캐시 resolve 지점을 바꿔
   진입 경로를 교란할 수 있음(단일 basic block이라 실제 x86엔 mid-block 진입 없음).
   → sentinel 기반 mid-block 증거는 비결정적. clean 덤프의 논리적 함의만 확실.
5. **트레이스 인프라 재사용**: `aot_transfer_trace`/`aot_return_trace`가 이미 리포트되어
   추가 코드 없이 제어흐름 provenance를 확인.

## 3. 핵심 발견 (Key Findings)

* 함수 0x03021DF8은 **asset-struct 준비 루틴**: 구조체(esi) memset → 파일명 목적지
  `[esp+0x154]=esi+0xC` 설정 → `[edi]`로 파일명 복사. 손상 슬롯은 이 파일명 목적지 포인터.
* 블록은 **memset 호출 + `test edi,edi; jle` 게이트 직후** fall-through로 진입(정상).
* 함수의 모든 call 반환이 **mispredict**(Tasks 219-220 thrashing) — fault 직전 반환은
  0x03021F36 착지.
* 핵심 역설 유지: 정상 진입·store 실행인데 clean 덤프에서 `[esp+0x154]`만 런 간 불변
  상수 `0xDD1523B1`(≠esi+0xC), 이웃은 esi 일관.

## 4. 다음 단계 (Next Step)

1. 함수 진입(0x03021DF8)~복귀로 **시간-게이팅된** `0x035D6B14` 관측(이 함수 실행 중
   ESP는 슬롯 아래로 내려가므로 Task 223의 상시-워치포인트 ESP-근접 문제를 회피할 안전
   구간이 있을 수 있음).
2. memset(0x030F4330)·반환-mispredict fallback 경로가 이 프레임 슬롯에 미치는 영향 검토.
3. `0xDD1523B1`이 인코딩하는 것(파일명 헤더/특정 상수) 식별.

---

**English summary.** Following Task 224's "EDX = corruption value" lead, fully mapped the
control flow around the corrupting block via `repiu_aot_probe` disassembly and the existing
AOT return/transfer traces (no code change). Findings: function `0x03021DF8` is an
asset-struct prep routine (memset the struct, set the filename destination pointer
`[esp+0x154]=esi+0xC`, copy the filename); the corrupting block is entered by fall-through
right after `call 0x030F4330` (a byte-fill memset) and a `test edi,edi; jle` gate; all of
this function's call returns are mispredicted (the Tasks 219-220 thrashing), with the last
before the fault landing at `0x03021F36`. The mid-block-entry reading from int3 sentinels is
not conclusive because in-block int3 can perturb the AOT inline-cache resolve point (a
single basic block has no real mid-block entry). Next steps: a time-gated observation of
`0x035D6B14` active only during this function's execution (a safe window may exist since ESP
descends below the slot during the call, avoiding Task 223's ESP-proximity failure), a
review of the memset and return-mispredict fallback paths, and identifying what
`0xDD1523B1` encodes.
