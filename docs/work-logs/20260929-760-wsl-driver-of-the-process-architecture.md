# Task 760 작업 로그 — WSL의 D3D12 드라이버를 프로세스의 아키텍처로 찾는다

설계: [20260929-760](../design/20260929-760-wsl-driver-of-the-process-architecture.md)
작업 지시: [20260929-760](../work-orders/20260929-760-wsl-driver-of-the-process-architecture.md)

## 요약

WSL에서 Linux i386 빌드가 창을 열지 못했습니다. Task 752의 드라이버 선택이 x86-64용 `d3d12_dri.so`를 보고
32비트 프로세스에 `GALLIUM_DRIVER=d3d12`를 설정했기 때문입니다. 드라이버 경로 목록을 아키텍처별로 나눠, 이제
i386 프로세스는 i386용 드라이버가 있을 때만 고릅니다. 이 머신에는 i386용이 없어 i386 빌드는 llvmpipe로 그리고,
x64 빌드는 그대로 D3D12로 그립니다.

## 검증

| 검증 | 결과 |
|---|---|
| Linux x64 Release 빌드 + core probe | 성공, `core_probe_all=true` |
| Linux i386 빌드 + core probe | 성공, `core_probe_all=true` |
| i386 pumpit1 25초(`REPIU_WSL_D3D12` 없음) | 수정 전: 시작 직후 `X_GLXCreateContext` 오류로 종료(종료 코드 1). 수정 후: 802프레임, `Glide GL renderer/wsl-d3d12-chosen: llvmpipe (LLVM 20.1.2, 256 bits)/false` |
| x64 pumpit1 25초 | 1,169프레임, `D3D12 (NVIDIA GeForce RTX 4090)/true` |

Win32는 이 파일을 빌드하지 않으므로 확인하지 않았습니다.

## 남은 것

* i386용 `d3d12_dri.so`가 있는 시스템에서의 동작은 확인하지 못했습니다(이 머신에 없음).
* x64 목록의 `/usr/lib/dri/`는 그대로 두었습니다. 32비트 전용 배포판에서 x64 빌드를 돌릴 수는 없으므로 그
  경로가 다른 아키텍처의 드라이버를 가리킬 일은 없다고 보았습니다.

---

# English

# Task 760 work log — finding WSL's D3D12 driver by the process's architecture

Design: [20260929-760](../design/20260929-760-wsl-driver-of-the-process-architecture.md)
Work order: [20260929-760](../work-orders/20260929-760-wsl-driver-of-the-process-architecture.md)

## Summary

On WSL the Linux i386 build could not open its window: Task 752's driver choice saw the x86-64
`d3d12_dri.so` and set `GALLIUM_DRIVER=d3d12` for a 32-bit process. The driver path list is now kept per
architecture, so an i386 process chooses only when there is an i386 driver. This machine has none, so the
i386 build draws with llvmpipe and the x64 build with D3D12 as before.

## Verification

| Check | Result |
|---|---|
| Linux x64 Release build + core probe | succeeded, `core_probe_all=true` |
| Linux i386 build + core probe | succeeded, `core_probe_all=true` |
| pumpit1 on i386 for 25 s (no `REPIU_WSL_D3D12`) | before: ended right after start on an `X_GLXCreateContext` error (exit code 1). After: 802 frames, `Glide GL renderer/wsl-d3d12-chosen: llvmpipe (LLVM 20.1.2, 256 bits)/false` |
| pumpit1 on x64 for 25 s | 1,169 frames, `D3D12 (NVIDIA GeForce RTX 4090)/true` |

Win32 does not build this file and was not checked.

## What remains

* Behaviour on a system that has an i386 `d3d12_dri.so` was not checked (this machine has none).
* `/usr/lib/dri/` stays in the x64 list. An x64 build cannot run on a 32-bit-only distribution, so that
  path is not expected to name another architecture's driver.
