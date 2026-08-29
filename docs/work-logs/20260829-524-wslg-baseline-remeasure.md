# Task 524 작업 로그 — WSLg 기준선 재측정

frontier: [linux-port-frontier](../analysis/linux-port-frontier.md) ·
절차: [execution-frame-rate-measurement](../guides/execution-frame-rate-measurement.md)

## 왜 쟀는가

사용자가 WSLg에서 "성능이 엄청 느려졌다"고 보고했습니다. v0.0.172 머지 직후였으므로 회귀를
의심할 근거가 있었습니다.

## 근인 — 회귀가 아니라 Debug 빌드

```
CMAKE_BUILD_TYPE:STRING=Debug
CXX_FLAGS = -m32 -g -std=c++20 -fno-pie      ← -O 플래그 없음
```

`CMakeCache.txt`가 머지 직후 새로 쓰였고, `scripts/build_linux_i386.sh`의 기본값이
`configuration="Debug"`입니다. `--config Release` 없이 부르면 이렇게 됩니다.
가이드에 Debug 계수 11.34배가 이미 기록돼 있습니다(Task 330).

## 측정 — Release 재빌드 후

`-O3 -DNDEBUG` 확인, 오브젝트·바이너리 재생성 확인 후 표준 절차대로 3회.

| | fps | 평균 | 프레임당 |
|---|---|---:|---:|
| 이전 기록 (Task 509) | 26.22 · 27.76 · 27.65 | 27.21 | 36.75 ms |
| **v0.0.172** | **34.11 · 35.22 · 36.96** | **35.43** | **28.2 ms** |

**1.30배 빠르고 두 집단이 겹치지 않습니다** (최저 34.11 > 최고 27.76).

## 배제한 것 — 추정이 아니라 확인

머지가 실행 경로에 넣은 것은 둘뿐이고 각각 이렇게 확인했습니다.

| 의심 | 확인 결과 |
|---|---|
| `HandleGuestOwnedBreakpoint`가 폴트마다 도는 비용 | `IsGuestInstructionPointer`·`IsAotCacheAddress` 둘 다 단순 범위 비교 |
| 그 핸들러가 다른 핸들러의 INT3를 가로챔 | 엔진이 심는 INT3는 전부 AOT 캐시 오프셋 → 걸러짐 |
| `REPIU_DOS_INT_TRACE` `getenv` | DOS 인터럽트는 15초 실행에 278회 |
| 컴파일 플래그 변경 | `CMakeLists.txt` 변경은 개명 + 새 소스 추가뿐 |

## 정정

조사 도중 "INT3 핸들러 배치를 성능 관점에서 다시 볼 여지가 있다"고 말했습니다.
**측정으로 반증됐습니다** — 회귀 자체가 없었습니다. 배치 문제는 실재하지만 성능이 아니라
정확성 사안이고, [Task 525](20260829-525-breakpoint-chain-order.md)에서 다룹니다.

## 남긴 경계

* **왜 빨라졌는지는 모릅니다.** 그 사이 성능을 노린 변경은 없었습니다. 확인하지 않았으므로
  원인을 추정하지 않습니다.
* **Windows는 다시 재지 않았습니다.** 730.05와 비교하면 26.8배 → 약 20.6배지만, 한쪽만 새로
  잰 비교이므로 그 배수는 원래 표만큼 단단하지 않습니다.
* **게임은 여전히 느립니다.** 20배 격차는 미해결이고 frontier 3.7절의 축 그대로입니다.
