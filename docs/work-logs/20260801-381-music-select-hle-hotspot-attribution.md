# 20260801-381 작업 로그: Music Select HLE 핫스팟 귀속 검토 / Work Log: Music Select HLE hotspot attribution review

설계: [20260801-381-music-select-hle-hotspot-attribution.md](../design/20260801-381-music-select-hle-hotspot-attribution.md)
작업 지시: [20260801-381-music-select-hle-hotspot-attribution.md](../work-orders/20260801-381-music-select-hle-hotspot-attribution.md)

## 한국어

### 확인된 결과

- 런타임 EIP는 probe 주소보다 `0x02000000` 높게 재배치되어 있었다. 원본 `PIU.EXE` 대조 결과, 높은 비용의 HLE 지점은 단일 Glide 호출이 아니라 다음 명령군의 혼합이다.
  - `0x030F3BAD`: `mov ds, edx`
  - `0x030F3BBD`: `pop ds`
  - `0x030F536A`: `mov eax, ds` (뒤따르는 `mov es, eax` 포함 경로)
  - `0x030F5637`: `int 21h`
  - `0x0303391A`: operand-size prefix가 붙은 `in ax, dx`
- 현재 `AotInstructionKind`에는 `kGuardedSegmentPop`과 `kPortIo`만 있으며 `kGuardedSegmentRead`는 없다. 계획 생성기와 code-cache emitter에도 segment-read 전용 분류·방출기는 없다. 따라서 `mov r32, Sreg`는 현재 `kHleBoundary`로 처리된다.
- Task 310의 설계와 작업 로그는 `kGuardedSegmentRead` 구현 및 런타임 검증을 주장한다. 그러나 두 문서를 추가한 커밋(`62c89f8`)의 직전 소스와 현재 소스 모두 그 enum, decoder, emitter를 포함하지 않는다. 이는 제거된 구현이 아니라 코드와 일치하지 않는 과거 문서 기록으로 분류한다.
- Port I/O는 `kPortIo`로 계획될 수 있지만, 현재 code cache는 이 명령도 일반 DBT HLE dispatch slot으로 방출한다. 즉 #DB VEH 진입은 피하더라도 HLE 처리 자체는 남는다.

### 결론

Music Select 지연의 남은 주요 원인은 shader GL error 검사나 단일 Glide 호출이 아니라, HLE로 처리되는 세그먼트 load/read, DOS `INT 21h`, port I/O의 누적 비용입니다. 특히 캡처에서 `HLE` 결과가 single-step 프로파일 주기의 약 98.43%를 차지했으므로, trace 재무장이나 native-span 조건 완화만으로는 체감 지연을 해소할 수 없습니다.

다음 검토는 각 명령군의 호출 횟수·서비스 번호·selector 일치율을 분리 계측하여, 안전한 segment-read fast-path와 DOS/port-I/O 특화 경로의 우선순위를 정하는 것입니다.

## English

### Confirmed results

- Runtime EIPs were relocated `0x02000000` above probe addresses. Matching them to the original `PIU.EXE` shows that expensive HLE sites are a mix rather than a single Glide call:
  - `0x030F3BAD`: `mov ds, edx`
  - `0x030F3BBD`: `pop ds`
  - `0x030F536A`: `mov eax, ds` (on a path followed by `mov es, eax`)
  - `0x030F5637`: `int 21h`
  - `0x0303391A`: operand-size-prefixed `in ax, dx`
- The current `AotInstructionKind` contains `kGuardedSegmentPop` and `kPortIo`, but not `kGuardedSegmentRead`. Neither the planner nor the code-cache emitter has a segment-read-specific classifier or emitter, so `mov r32, Sreg` currently takes `kHleBoundary`.
- Task 310's design and work log claim an implemented and runtime-verified `kGuardedSegmentRead`. The source immediately before the commit that added those documents (`62c89f8`), as well as current source, contains no such enum, decoder, or emitter. This is therefore a historical documentation record inconsistent with code, not a later-removed implementation.
- Port I/O can be classified as `kPortIo`, but the current code cache still emits a general DBT HLE-dispatch slot. It avoids #DB VEH entry but retains HLE work.

### Conclusion

The remaining Music Select delay is accumulated HLE work for segment loads/reads, DOS `INT 21h`, and port I/O, not shader GL-error checks or one Glide call. Since `HLE` outcomes account for about 98.43% of single-step-profile cycles in the capture, trace re-arming or native-span relaxation alone cannot resolve the visible delay.

The next review should separately measure call count, service number, and selector-match rate for each family to prioritize a safe segment-read fast path and specialized DOS/port-I/O paths.
