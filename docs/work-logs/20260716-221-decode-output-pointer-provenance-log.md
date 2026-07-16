# 디코드 출력 포인터 provenance 확정과 resize 32비트 수정 작업 로그
# Work Log: Decode Output Pointer Provenance Confirmed; 32-bit Resize Fix

## 1. 확정된 원인 (Root Cause)

`0x045D3EB0` overflow의 출처는 `INT 21h AH=4Ah`가 맞았으나 Task 213의 모델이 두 겹으로
무력화되어 있었다:

1. **EBX 32비트 절단:** 게스트(DOS/4G 확장 API)는 EBX에 32비트 paragraph 수(실측
   `0x36B300`~`0x533500` ≈ 55~83MB)를 넘기는데 핸들러가 하위 16비트만 읽어 소형 요청으로
   오인, 전부 무조건 성공 처리했다.
2. **base 0 해석:** ES가 호스트 진입 selector(`0x2B`)로 남아 `FindDescriptor` 실패 →
   `selector_base=0` → 상한 비교가 항상 통과.

결과: 게스트 allocator는 heap이 무한하다고 믿고 커밋 영역(arena end `0x045D7000`)을 넘는
블록을 배정, 디코드 스토어가 fault했다.

## 2. 수정 (Fix)

1. `HandleDosResizeMemoryBlock`(`execution_trampoline.cpp`): EBX 전체를 크기로 해석(uint64
   overflow 가드), ES는 `ReadGuestSegmentSelector`로 해석하되 base가 0이면 프로그램 블록
   base(`runtime_base`)로 폴백, 거절 시 최대 paragraph 수를 **EBX 전체(32비트)**로 반환.
2. 라이브 텔레메트리 v16: `dos_resize_count/reject_count/last_ebx/last_selector/last_base`.
3. `kRuntimeArenaExpansionSlack` `0x01000000` → `0x08000000`(128MB): 게임의 실제 peak 수요가
   약 83MB(`0x533500` paragraphs)로 실측되어, 상한 거절 대신 진짜 커밋 메모리로 성공시킨다.

## 3. 검증 (Verification, aot-dynamic 60초)

| slack | resize 결과 | 게스트 거동 |
|---|---|---|
| 16MB(수정 전) | 150건 전부 무조건 성공(절단) | `0x030873F4`→`0x045D3EB0` overflow로 종료 |
| 16MB(상한 실동작) | 3/3 거절 | 메모리 부족으로 **정상 자진 종료**(4초) |
| 64MB | 151건 중 2건 거절(70MB 요구) | `0x030579AE` 읽기 예외(신규 지점)로 종료 |
| **128MB(최종)** | **212건, 거절 0, peak 0x533500(≈83MB)** | 39초까지 전진 후 `0x0302208C`(`mov [edi],al` 복사 스토어, 인접 창에 `INT 21h AH=3Fh` 파일 읽기)에서 종료, `child_exit=0` 정상 회수 |

기존 종료 지점 `0x030873F4`/`0x045D3EB0`은 소멸. 실행이 구동마다 더 깊은 자산 적재 단계로
전진했다.

## 4. 남은 확인 (Open)

1. 새 frontier `0x0302208C` 복사 스토어의 대상 주소/조건 분석(다음 태스크).
2. 기본 trap 백엔드 30초 회귀 확인(이번 세션 미수행 — 다음 태스크 시작 시 함께 수행 예정).
3. resize 시 ES가 여전히 `0x2B`로 읽히는 근본 원인(shadow 미갱신)은 별도 주제로 남김.

**Confirmed (Task 221):** the `0x045D3EB0` overflow's provenance was INT 21h AH=4Ah after all,
but doubly disabled: the guest passes 32-bit paragraph counts in EBX (measured up to `0x533500`
≈ 83 MiB) that the handler truncated to 16 bits, and ES resolves to the host selector `0x2B`
(base 0), so Task 213's ceiling never rejected anything and the guest believed its heap was
unbounded. Fix: honor full 32-bit EBX, fall back to `runtime_base` as the block base, return the
32-bit max on rejection (telemetry v16), and raise the arena expansion slack to 128 MiB since the
game's measured peak demand is ~83 MiB. With the fix the old terminal store is gone, the guest
advances further each run, and now terminates at a new frontier (`0x0302208C` byte-copy store
near an INT 21h AH=3Fh file-read path) at ~39 s with a clean loader exit. Open: analyze the new
frontier; run the trap-backend regression; the stale ES shadow at resize time remains a separate
topic.
