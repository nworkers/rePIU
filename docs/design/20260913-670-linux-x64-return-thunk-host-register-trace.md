# Task 670 설계: Linux x64 ReturnThunk host register trace

## 한국어

### 목적

Task 669 이후 Linux x64 실행은 DOS `AH=43h` 파일 속성 조회와 `.ovl` 로딩을 통과하지만, 장시간 실행 후 `RepiuLinuxX64ReturnThunk`의 첫 `PUSHFQ`에서 저주소 `RSP`로 `SIGSEGV`가 발생합니다. 현재 fault 출력에는 guest 레지스터와 host `RIP/RSP`만 있어, 해당 ReturnThunk 진입을 만든 guest producer와 resolver target을 구분할 수 없습니다.

### 설계 판단

- Linux x64 signal context에서 host `R10`, `R14`, `R15`를 읽어 fault 진단에 추가합니다.
- `R10D`는 현재 x64 return/indirect-call emitter가 ReturnThunk에 전달하는 producer tag입니다.
- `R14D`는 ReturnThunk 진입 시 resolve 대상 guest 주소입니다.
- `R15D`는 guest ESP이며, host `RSP`와 분리되어야 합니다.
- 진단은 unhandled fault 출력에만 추가하고, guest context 저장·복구나 ReturnThunk 동작은 변경하지 않습니다.
- x86 및 비-Linux 빌드에서는 기존 출력과 동작을 유지합니다.

### 검증 전략

1. Linux x64 `repiu`를 재빌드합니다.
2. `repiu_core_probe`를 실행해 공용 lowering 회귀가 없는지 확인합니다.
3. `pumpit2a`를 bounded 실행하고 fault line의 `host_r10`, `host_r14`, `host_r15`를 확인합니다.

### 기대 결과

fault가 ReturnThunk 진입 직후 발생하면 producer tag와 resolve target이 보입니다. 이 값으로 다음 분석을 특정 return/call producer의 원본·emitted slot으로 연결할 수 있습니다. 이 작업 자체는 특정 EIP 예외처리를 추가하지 않습니다.

## English

### Purpose

After Task 669, Linux x64 execution passes the DOS `AH=43h` attribute lookup and `.ovl` loading, but a long run eventually faults with `SIGSEGV` on the first `PUSHFQ` in `RepiuLinuxX64ReturnThunk`, using a low host `RSP`. The current fault line has guest registers and host `RIP/RSP`, but not the guest producer or resolver target that entered the thunk.

### Design decisions

- Read host `R10`, `R14`, and `R15` from the Linux x64 signal context and add them to fault diagnostics.
- `R10D` is the producer tag passed by the x64 return/indirect-call emitter.
- `R14D` is the guest address to resolve on ReturnThunk entry.
- `R15D` is guest ESP and must remain separate from host `RSP`.
- Add the data only to unhandled-fault output; do not change guest context save/restore or ReturnThunk behavior.
- Preserve existing output and behavior on x86 and non-Linux builds.

### Verification strategy

1. Rebuild the Linux x64 `repiu` target.
2. Run `repiu_core_probe` to check for common lowering regressions.
3. Run bounded `pumpit2a` and inspect `host_r10`, `host_r14`, and `host_r15` on the fault line.

### Expected result

If the fault occurs immediately on ReturnThunk entry, the producer tag and resolve target will identify the entry path. Those values can then connect the next analysis to one return/call producer and its original/emitted slot. This task does not add an EIP-specific exception.
