# Linux x64 coredump 복구 작업 지시서

## 한국어

### 목표

Linux x64에서 timeout 또는 stall recovery가 의도하지 않은 SIGILL/SIGSEGV와
coredump로 끝나지 않도록 공용 host-thread interrupt context 경로를 추가하고
검증한다. 동시에 실행 중 최초 AOT planner-HLE 경계의 원인을 기록하여 다음
수정의 범위를 확정한다.

### 작업 범위

1. `host_thread`에 native context callback API를 추가한다.
2. Linux signal handler와 Windows suspend path가 새 callback을 전달한다.
3. Linux x64 shutdown recovery가 full native RIP와 기존 guest context를
   함께 갱신하도록 연결한다.
4. 3초 timeout, 15초 실행 재현, core probe 및 기존 Linux x64 빌드로 검증한다.
5. 16-bit address-size AOT 경계의 분류·원본 바이트·register state를 작업
   로그와 analysis 문서에 남긴다. 이 작업에서 그 명령을 주소별로 예외처리하지
   않는다.

### 완료 조건

- Linux x64 Debug가 빌드된다.
- timeout 경로에서 `RecoverGuestStackException`의 UD2로 인한 coredump가
  재현되지 않고, cache exit를 통한 종료 또는 명시된 안전한 실패가 관찰된다.
- `repiu_core_probe`가 통과한다.
- 15초 실행 결과가 shutdown fault와 실행 중 AOT boundary를 구분해 기록한다.
- 대응 work-log가 남고 관련 커밋이 생성된다.

### 검증 명령

```bash
cd /mnt/e/MYWORK/Projects/rePIU
scripts/build_linux_x64.sh --config Debug --build-dir build/linux_x64_debug --target repiu
cmake --build build/linux_x64_debug --target repiu_core_probe
./build/linux_x64_debug/repiu_core_probe
REPIU_EXECUTION_BACKEND=dynamic REPIU_LAUNCHER=0 \
REPIU_EXECUTION_TIMEOUT_MS=3000 REPIU_STALL_TIMEOUT_MS=0 \
timeout -k 3s 10s ./build/linux_x64_debug/repiu pumpit2a
```

## English

### Goal

Add and verify a shared host-thread interrupt-context path so Linux x64 timeout
or stall recovery does not end in an unintended SIGILL/SIGSEGV and coredump.
Also record the first runtime AOT planner-HLE boundary so the next change has a
defined scope.

### Scope

1. Add a native-context callback API to `host_thread`.
2. Forward the new callback from the Linux signal handler and Windows suspend
   path.
3. Connect Linux x64 shutdown recovery so it updates full native RIP together
   with the existing guest context.
4. Verify with the 3-second timeout, 15-second execution reproduction, core
   probe, and the existing Linux x64 build.
5. Record the 16-bit address-size AOT boundary's classification, source bytes,
   and register state in the work log and analysis documents. Do not add an
   address-specific exception in this task.

### Completion criteria

- Linux x64 Debug builds.
- The timeout path no longer reaches the intentional UD2 in
  `RecoverGuestStackException`; it either exits through cache exit or reports a
  documented safe failure.
- `repiu_core_probe` passes.
- The 15-second result distinguishes shutdown faults from the runtime AOT
  boundary.
- A work log and corresponding commit exist.

### Verification commands

Use the commands shown in the Korean section above from WSL.
