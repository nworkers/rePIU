# Task 760: WSL 드라이버를 프로세스의 아키텍처로 작업 지시

설계: [20260929-760](../design/20260929-760-wsl-driver-of-the-process-architecture.md)

## 한국어

1. `src/platform/linux/host_gpu_driver_arch.h`와 `x64/`, `x86/`의 `host_gpu_driver_arch.cpp`에 드라이버 경로
   목록을 둔다.
2. `host_gpu_driver.cpp`가 그 목록에서 찾는다.
3. 두 아키텍처를 빌드·실행해 확인하고 README와 작업 로그를 갱신한 뒤 커밋한다.

## English

1. Put the driver path lists in `src/platform/linux/host_gpu_driver_arch.h` and the
   `host_gpu_driver_arch.cpp` of `x64/` and `x86/`.
2. `host_gpu_driver.cpp` searches that list.
3. Build and run both architectures, update README and the work log, and commit.
