# Task 720 작업 지시 — Linux x64 Glide gate 직접 디스패치

설계: [20260919-720](../design/20260919-720-linux-x64-glide-direct-dispatch.md)

## 범위

Linux x64에서 Glide gate를 `ud2` trap 없이 호출한다. Win32 x86은 바꾸지 않는다.

## 단계

1. `RepiuLinuxX64GlideGateThunk`(.S)와 resolver 포인터 설치 함수
2. engine resolver `ResolveLinuxX64GlideGateFrame`, x64 thunk 주소 반환
3. 게스트 진입 때 resolver 설치
4. probe
5. 두 host 빌드·core probe, Linux 180초 장면 기록, Win32 확인
6. 작업 로그, frontier

## 검증

* Linux x64 core probe, Win32 core probe
* Linux: 폴트 0, `ud2` boundary와 swap 정지 감소

---

## English

Design: [20260919-720](../design/20260919-720-linux-x64-glide-direct-dispatch.md)

### Scope

Call Glide gates on Linux x64 without the `ud2` trap. Win32 x86 does not change.

### Steps

1. `RepiuLinuxX64GlideGateThunk` (.S) and a function to install its resolver pointer.
2. Engine resolver `ResolveLinuxX64GlideGateFrame`; return the x64 thunk address.
3. Install the resolver at guest entry.
4. Probe.
5. Builds and core probes on both hosts, a 180-second Linux scene recording, Win32
   check.
6. Work log and frontier.

### Verification

* Linux x64 and Win32 core probes.
* Linux: no faults; fewer `ud2` boundaries and swap stalls.
