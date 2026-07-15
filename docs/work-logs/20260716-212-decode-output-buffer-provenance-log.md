# 작업 로그: 디코드 출력 버퍼 0x045D3EB0 provenance 진단 (Task 212)

* 작업 지시: `docs/work-orders/20260716-212-decode-output-buffer-provenance-order.md`
* 설계: `docs/design/20260716-212-decode-output-buffer-provenance.md`
* 브랜치: `feature/212-decode-output-buffer-provenance`

## 1. 결론 요약

Task 205~206의 종료 예외("`0x045D3EB0` 미매핑 스토어") 판정을 실측으로 재판정했다. 실제 fault는 **arena 끝 `0x045D7000`(MEM_FREE)에 대한 쓰기**이며, 디코드 루프는 버퍼 base `0x045D3EB0`부터 `0x3150`바이트를 정상적으로 쓴 뒤 arena 경계를 넘는 순간 죽는다 — **arena-end overflow**다. 또한 버퍼 base는 LINEXE 합성 private data 영역(`0x045D2000`~) 내부여서, fault 전까지의 쓰기가 합성 데이터를 조용히 훼손하고 있었다.

## 2. 구현 (진단 계측, 관찰 전용)

SEH 필터 `CaptureException`(게스트 스레드, 크래시 순간)에 추가:

* `ExceptionInformation[0]/[1]` — 접근 종류와 정확한 fault VA.
* fault VA의 `VirtualQuery` — region base / allocation base / State / Protect / region size.
* `[ESI+0x20..+0x3C]` 8 dword checked 덤프(`ReadProcessMemory` self).

`ThreadContext`/attempt 필드와 요약 로그 3종(`exception access/fault VA`, `exception fault page ...`, `exception ESI structure ...`)을 추가했다.

## 3. 실측 결과 (aot-dynamic 180초, 게스트 종료 ~147초)

```
exception access/fault VA:        1 (write) / 0x045D7000
fault page base/alloc/state/protect/size: 0x045D7000 / (null) / MEM_FREE(0x10000) / 0x01 / 0x9000
ESI structure +0x20..+0x3C:       1, 8, 8, 0, 8, 0x045D3EB0, 0, 0x0001D2A0
예외 시점 EBP:                    0x3150 (인덱스)
```

* `[ESI+0x34] = 0x045D3EB0` — 출력 버퍼 base 확인. `[ESI+0x3C] = 0x1D2A0`(약 117 KiB)는 총 출력 크기로 추정.
* base부터 arena 끝(`0x045D7000`)까지 여유는 `0x3150`바이트뿐 → **게임 allocator가 arena 끝 너머까지 소유했다고 믿고 블록을 배정**했다.
* 기준선 거동 유지 확인(회귀 없음): Glide 창 1회, MSCDEX request 1건 처리, 종료 지점 동일.

## 4. 구조적 배경 (정적 확인)

* `PlaceWin32RelocatedImage`는 전체 `0x015D7000`을 `MEM_COMMIT|PAGE_READWRITE`로 커밋한다 — arena 안 쓰기는 fault하지 않으므로 "미매핑" 판정은 성립할 수 없었다.
* `BuildLinexeArenaLayout`은 arena 끝에서 아래로 client(4 KiB)/gate/bss/private data를 배치하고 게임 allocator 상한을 `dynamic_allocator_end = client_data_base(0x045C6000)`로 설계했으나, 이 상한이 게임 allocator에 전달되는 메커니즘이 없다.
* `INT 21h AH=4Ah` resize HLE는 크기 추적 없이 사실상 무조건 성공을 보고한다.

## 5. 후속 작업 제안 (수정은 본 작업 범위에서 분리)

1. **allocator heap 상한의 출처 역추적** — resize 응답인지, 합성 DOS/4G client/private data의 메모리 풀 경계 값인지.
2. 방향 결정: **(a) allocator가 보는 상한을 `dynamic_allocator_end`로 정확히 모델링(권장 — 정확성 우선 원칙, LINEXE 충돌 동시 해소)** vs (b) arena expansion slack 확장(현 16 MiB; 충돌·무한 성장 문제 잔존).
3. 부차 미확정: 과거 "applied"로 기록된 `0x045D3EAC`/`0x045D3FFF` boundary store가 RW 페이지에서 fault했던 이유(당시 write-watch 보호 여부).

---

# Work Log: Decode Output Buffer 0x045D3EB0 Provenance Diagnosis (Task 212)

**Conclusion.** The Task 205–206 terminal-exception reading ("unmapped store at `0x045D3EB0`") is re-attributed by direct measurement: the real fault is a **write to `0x045D7000` — the end of the runtime arena** (`MEM_FREE`, no allocation base). With `EBP=0x3150` at the exception, the decode loop successfully wrote `0x3150` bytes from buffer base `0x045D3EB0` and died crossing the arena boundary — an arena-end overflow. The buffer base also lies inside the LINEXE synthetic private-data region, so the pre-fault writes silently corrupt it.

**Implementation (observation-only).** `CaptureException` (the SEH filter, at crash time in the guest thread) now records `ExceptionInformation[0]/[1]`, a `VirtualQuery` of the fault VA, and a checked `[ESI+0x20..0x3C]` dump; three new summary lines expose them.

**Measurement (aot-dynamic 180 s, guest death ~147 s).** Fault: write / `0x045D7000`; page: `MEM_FREE`, region `0x9000`. Structure: `[+0x34]=0x045D3EB0` (buffer base), `[+0x3C]=0x0001D2A0` (~117 KiB presumed total output). Only `0x3150` bytes fit between the base and the arena end, so the guest allocator handed out a block it believes extends past the arena. Baseline behavior unchanged (window, MSCDEX request handled, same death site).

**Structural background.** Placement commits the entire `0x015D7000` as RW; `BuildLinexeArenaLayout` designs the allocator ceiling as `dynamic_allocator_end = 0x045C6000` but nothing conveys it to the guest allocator; the `AH=4Ah` resize HLE reports success unconditionally.

**Follow-up.** Trace where the allocator's heap-top belief comes from (resize replies vs. synthesized DOS/4G pool bounds), then choose between (a) modeling the real ceiling at `dynamic_allocator_end` (recommended — accuracy-first, also resolves the LINEXE collision) and (b) enlarging the expansion slack (collision and unbounded growth remain). Minor open item: why the previously "applied" boundary stores at `0x045D3EAC`/`0x045D3FFF` faulted on RW pages at all.
