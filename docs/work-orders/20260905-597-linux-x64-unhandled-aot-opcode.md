# 작업 지시 20260905-597 — Linux x64 unhandled AOT fault opcode 진단

설계: [20260905-597](../design/20260905-597-linux-x64-unhandled-aot-opcode.md)

## 목표

Linux x64에서 처리되지 않은 AOT cache SIGSEGV가 발생할 때 faulting host
RIP의 실제 명령 바이트를 안전하게 기록하여 `0x2004FB6B` blocker의 성격을
확정합니다.

## 작업

1. Linux fault reporter에 signal-safe host opcode read helper를 추가합니다.
2. unhandled access fault 출력에 최대 16바이트의 `bytes=` 필드를 추가합니다.
3. `REPIU_AOT_FAULT_TRACE=1`일 때 faulting cache 주소의 reverse map 상태를
   출력합니다.
4. Linux x64 `repiu`와 `repiu_core_probe`를 빌드합니다.
5. `repiu_core_probe`와 짧은 `pumpit2a`를 실행합니다.
6. unhandled fault의 guest stack window(`ESP-8`부터 4 dword)를 기록하여
   faulting instruction과 nearby stack 상태를 대조합니다.
7. 실제 opcode, map 상태와 판단을 `docs/analysis/linux-port-frontier.md` 및 작업 로그에
   기록합니다.

## 완료 기준

* core probe가 `core_probe_failures=0`으로 종료됩니다.
* 기존 fault 주소와 GPR 출력이 유지됩니다.
* `pumpit2a` unhandled fault line에 `bytes=`가 출력됩니다.
* `0x2004FB6B`의 opcode가 확인되거나, 읽기 불가 사유가 명확히 기록됩니다.
* opt-in AOT fault trace가 map 등록 여부와 현재 cache size를 기록합니다.
* guest stack window가 읽히고, `EBX=0`의 upstream에 대해 확인된 범위와
  미확정 범위가 명확히 기록됩니다.
* 설계·작업 지시·작업 로그가 모두 남습니다.

## English

# Work order 20260905-597 — Linux x64 unhandled AOT fault opcode diagnostics

Design: [20260905-597](../design/20260905-597-linux-x64-unhandled-aot-opcode.md)

## Objective

When an unhandled AOT-cache SIGSEGV occurs on Linux x64, safely record the
actual instruction bytes at the faulting host RIP so the `0x2004FB6B` blocker
can be classified.

## Work

1. Add a signal-safe host-opcode read helper to the Linux fault reporter.
2. Add up to 16 bytes as a `bytes=` field on unhandled access-fault output.
3. When `REPIU_AOT_FAULT_TRACE=1`, print reverse-map status for the faulting
   cache address.
4. Build Linux x64 `repiu` and `repiu_core_probe`.
5. Run the core probe and a short `pumpit2a` sample.
6. Record the guest stack window (four dwords from `ESP-8`) and correlate it
   with the actual faulting instruction.
7. Record the opcode, map status, and assessment in `docs/analysis/linux-port-frontier.md`
   and the work log.

## Completion criteria

* The core probe exits with `core_probe_failures=0`.
* Existing fault address and GPR output remain present.
* The `pumpit2a` unhandled fault line contains `bytes=`.
* The opcode at `0x2004FB6B` is identified, or the read failure is explained.
* The opt-in AOT fault trace records map registration and current cache size.
* The guest stack window is recorded and the confirmed versus unresolved parts
  of the upstream source for `EBX=0` are stated.
* Design, work order, and work log are present.
