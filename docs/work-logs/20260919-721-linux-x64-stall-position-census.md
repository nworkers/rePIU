# Task 721 작업 로그 — Linux x64 정지 구간 guest 위치 census

설계: [20260919-721](../design/20260919-721-linux-x64-stall-position-census.md) ·
작업 지시: [20260919-721](../work-orders/20260919-721-linux-x64-stall-position-census.md) ·
frontier: [linux-port-frontier](../analysis/linux-port-frontier.md) ·
선행: [20260919-720](20260919-720-linux-x64-glide-direct-dispatch.md)

## 수행 결과

Linux x64 `CaptureNativePhaseSample`을
`REPIU_LINUX_X64_NATIVE_SAMPLE=1` opt-in으로 활성화했습니다. callback은
`GuestCpuContext`를 읽기만 하고 false를 반환하므로 Task 705의 native-context
no-write-back 계약을 유지합니다. Linux x64 signal handler에서는 `process_vm_readv`를 쓰는
host-stack scan을 수행하지 않습니다.

## 검증

- Linux x64 Debug `repiu`, `repiu_core_probe` 빌드 성공, core probe 30/30 성공
- Win32 x86 Debug 전체 빌드 성공, core probe 28/28 성공
- 기본 Linux 30초 실행: x64 opt-in이 없으면 sampler가 `stage=3`에서 반환하며 signal을
  보내지 않음
- opt-in `pumpit2a` 35초 관찰(100ms): capture 345, distinct 200, overflow 0,
  capture failure 0, fault 없이 timeout immediate-exit
- 짧은 opt-in 관찰: capture 117, host 112, cache-mapped 5, host-stack site 0

## 결론

관찰 capture 자체는 안정적이지만, host RIP가 guest ABI의 32-bit EIP로 절단되어 host 대기
위치를 식별할 수 없습니다. 다음 작업은 full host RIP와 시간 순 표본을 별도 관찰 ABI로
보존합니다.

---

## English

Design: [20260919-721](../design/20260919-721-linux-x64-stall-position-census.md) ·
Work order: [20260919-721](../work-orders/20260919-721-linux-x64-stall-position-census.md) ·
Frontier: [linux-port-frontier](../analysis/linux-port-frontier.md) ·
Predecessor: [20260919-720](20260919-720-linux-x64-glide-direct-dispatch.md)

### Result

Linux x64 `CaptureNativePhaseSample` is enabled only with the
`REPIU_LINUX_X64_NATIVE_SAMPLE=1` opt-in. Its callback reads `GuestCpuContext` and
returns false, retaining Task 705's no-write-back contract. The Linux x64 signal
handler does not perform the `process_vm_readv` host-stack scan.

### Verification

- Linux x64 Debug `repiu` and `repiu_core_probe` built; core probe passed 30/30.
- Full Win32 x86 Debug build succeeded; core probe passed 28/28.
- In the default Linux 30-second run, no x64 opt-in means the sampler returns at
  `stage=3` without sending a signal.
- A 35-second opt-in `pumpit2a` observation at 100ms captured 345 samples with
  200 distinct positions, zero overflow, zero capture failures, and fault-free
  timeout immediate-exit.
- A short opt-in observation captured 117 samples: 112 host, five cache-mapped,
  and zero host-stack sites.

### Conclusion

Capture itself is stable, but host RIP is truncated into the guest ABI's 32-bit EIP,
so a host wait location cannot be identified. The next task preserves full host RIP
and time-ordered samples in a separate observation ABI.
