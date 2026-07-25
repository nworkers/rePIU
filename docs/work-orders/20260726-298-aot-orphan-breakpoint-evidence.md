# 20260726-298 작업 지시: AOT orphan breakpoint 증거 캡처 / Work order

설계: [docs/design/20260726-298-aot-orphan-breakpoint-evidence.md](../design/20260726-298-aot-orphan-breakpoint-evidence.md)

## 한국어

### 목표

처리되지 않은 `STATUS_BREAKPOINT`가 기존 fail-closed 종료 동작을 유지하면서 원시
예외/AOT 상태를 자체 완결 로그로 남기게 합니다.

### 구현 항목

1. `Win32UnhandledBreakpointEvidence` 공개 결과 구조를 추가합니다.
2. `breakpoint_evidence_win32.h/.cpp`에 다음을 구현합니다.
   - breakpoint 진입 스냅샷
   - exact/previous AOT 매핑 및 provenance
   - 원시 주소/EIP byte window와 stack top 캡처
   - 최종 unhandled 상태 커밋
3. `DispatchGuestException`에서 포인터 검증 후 캡처하고, 최종
   `CaptureException` 직전에만 커밋합니다.
4. `CopyThreadObservationToAttempt`가 증거 구조를 결과로 복사하게 합니다.
5. Win32 loader summary에 증거를 구조화해 출력합니다.
6. CMake Win32 source 목록에 새 구현 파일을 추가합니다.

### 안전 조건

- 기존 breakpoint 처리 순서와 반환값을 변경하지 않습니다.
- `INT3`, `RET`, stack pointer 또는 EIP를 수정하지 않습니다.
- VEH 계측에서 heap 할당과 파일 I/O를 하지 않습니다.
- 읽을 수 없는 주소는 fail-closed하고 valid count/mask만 기록합니다.
- 사용자 소유 `repiu_log.txt`는 수정하거나 커밋하지 않습니다.

### 검증

1. `cmake --build build/win32_x86_debug --config Debug --target repiu_loader_win32`
2. 새 계측 파일에서 allocation/string/file API가 사용되지 않는지 정적 검색
3. loader summary의 모든 증거 필드가 빌드에 연결됐는지 정적 확인
4. 실제 장기 재현은 후속 사용자 실행 로그로 판정

---

## English

### Goal

Preserve the existing fail-closed behavior while making an unhandled
`STATUS_BREAKPOINT` emit a self-contained raw exception/AOT evidence packet.

### Implementation

1. Add the public `Win32UnhandledBreakpointEvidence` result structure.
2. Implement entry capture, exact/previous AOT mapping and provenance, raw/EIP
   byte windows, stack-top capture, and final commit in
   `breakpoint_evidence_win32.h/.cpp`.
3. Capture after pointer validation in `DispatchGuestException` and commit only
   immediately before final `CaptureException`.
4. Copy the structure through `CopyThreadObservationToAttempt`.
5. Print structured evidence in the Win32 loader summary.
6. Add the implementation source to the Win32 CMake source list.

### Safety

- Do not change breakpoint handler order or return values.
- Do not modify `INT3`, `RET`, ESP, or EIP.
- Do not allocate or perform file I/O in the VEH instrumentation.
- Fail closed on unreadable addresses and record only valid counts/masks.
- Do not modify or commit the user-owned `repiu_log.txt`.

### Verification

1. Build `repiu_loader_win32` in Win32 x86 Debug.
2. Statically check the new instrumentation for allocation/string/file APIs.
3. Confirm all evidence fields are wired to the loader summary.
4. Use the next user-produced long-run log for runtime confirmation.
