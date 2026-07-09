# Traced shadow memory load HLE 작업 지시

## 목표

out-of-arena store shadow memory를 추가하고, `0x0201DF24`의 `8B /r` dword memory load를 처리해 다음 진행 지점을 확인한다.

## 범위

* ThreadContext에 byte-addressed shadow memory를 추가한다.
* skipped out-of-arena stores를 shadow memory에 기록한다.
* `8B /r` 중 SIB 없는 memory source와 register destination 형태를 처리한다.
* runtime arena 내부 source는 실제 read로 처리한다.
* runtime arena 외부 source는 shadow memory에 모든 byte가 있을 때만 처리한다.
* 테스트 기대 관측 지점, 작업 로그, TODO를 갱신한다.

## 검증

* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`

# Traced Shadow Memory Load HLE Work Order

## Goal

Add out-of-arena store shadow memory and handle the `8B /r` dword memory load at `0x0201DF24` to identify the next reachable point.

## Scope

* Add byte-addressed shadow memory to `ThreadContext`.
* Record skipped out-of-arena stores into shadow memory.
* Handle `8B /r` forms with memory source and register destination without SIB.
* Use actual reads for sources inside the runtime arena.
* Use shadow memory for out-of-arena sources only when every byte is present.
* Update the expected test observation point, work log, and TODO.

## Verification

* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`
