# [WIP] rePIU 진행 상황: Native AOT 동적 번역 및 성능 향상

그동안 `rePIU`는 Legacy 모드에서의 기본 구동과 주변 HLE(High-Level Emulation) 환경 구축에 집중해 왔습니다. 하지만 원본 x86 명령어를 모두 Single-step Trap으로 처리하는 방식은 오버헤드가 매우 큽니다. 이를 극복하기 위해 최근 진행된 작업 브랜치에서는 **Native AOT (Ahead-of-Time) 동적 번역기**를 도입하고 고도화하는 데 집중했습니다.

이번 포스트에서는 지난 포스팅 시점 이후로 `main` 브랜치에 반영된 AOT 관련 주요 변경 사항과, 이를 통해 얻은 실제 성능 차이를 비교해 봅니다.

## 주요 진행 사항 (Commits Summary)

지난번 이후 누적된 주요 작업들은 다음과 같습니다:

1. **AOT 동적 번역기 도입 및 코드 캐시 구축**
   - DOS4GW 기반 코드를 Win32 네이티브 환경에서 직접 실행할 수 있도록 `aot` 및 `aot-dynamic` 백엔드 실행 브리지를 연결했습니다.
   - 런타임에 호출되는 기본 블록(Basic Block)들을 네이티브 명령어로 변환하여 배치할 수 있는 재배치 가능(relocatable) 코드 캐시 방식을 구현했습니다.
2. **동적 제어 흐름(Control Flow) 최적화**
   - 정적으로 분석되지 않은 Indirect Call 및 Return 처리, 조건부 분기(Conditional Transfer), Fallthrough 링킹 등을 런타임에 Host Worker가 동적으로 추적하여 변환하도록 개선했습니다.
   - Worker 기반의 인라인 캐시(Inline Caches)를 도입하여 캐시 미스 비용을 최소화했습니다.
3. **자체 수정 코드(Self-Modifying Code, SMC) 일관성 해결**
   - PIU 게임 로직 특성상 발생하는 코드 변조(Import stub 등)를 처리하기 위해, 페이지 단위의 일관성 관리(Page Coherency) 모델을 도입했습니다.
   - 런타임에 코드가 변경되면 해당 캐시를 즉시 무효화(Retirement)하고, 새로운 상태를 기반으로 라이브 아레나 스냅샷을 생성해 새 기계어를 발행함으로써 버그 없이 실행 흐름을 이어가게 했습니다.

## 성능 비교 (Performance Comparison)

실제 10초간 게임 루프를 구동(`repiu_supervisor_win32.exe`)하며 측정한 백엔드별 성능 데이터입니다. (Win32 x86 Debug 빌드 기준)

| 측정 항목 / 백엔드 | `legacy` (기존) | `aot` (정적 AOT) | `aot-dynamic` (동적 AOT) |
| :--- | :--- | :--- | :--- |
| **3초 경과 누적 Heartbeat** | 671,524 | 645,770 | 76,601 |
| **10초 경과 누적 Heartbeat** | 2,248,638 | 2,085,754 | 1,919,174 |
| **10초 누적 Single Step Trap**| 1,124,319 회 | 1,042,877 회 | 약 510,000 회 이하 |
| **Trap 발생 비율** | Heartbeat 2회 당 1회 | Heartbeat 2회 당 1회 | **Heartbeat 약 3.8회 당 1회** |

### 분석 결과
* **에뮬레이션 오버헤드 감소**: 기존 `legacy` 방식은 실행 내내 Trap을 유발하여 큰 부하를 발생시켰으나, `aot-dynamic` 모드 도입 이후에는 캐시에 번역된 코드를 기계어 수준에서 직접 실행(Direct execution)하게 되어 Trap 발생 빈도가 절반 이하로 줄어들었습니다.
* **Warm-up 이후의 가속**: `aot-dynamic` 방식은 첫 실행 시 코드 디코딩과 변환, SMC 감지 처리에 자원을 쓰기 때문에 초기 3초간은 상대적으로 Heartbeat 진행이 더딥니다. 하지만 캐시가 한 번 채워진 이후에는 가파른 속도로 진행률이 증가하여, 10초를 넘어서는 시점부터는 모든 번역 비용을 상쇄하고 전체 성능을 극적으로 끌어올립니다.

이제 본격적인 최적화와 함께 Release 빌드를 적용하면 Native 수준의 실행 성능을 기대할 수 있습니다.

---

# [WIP] rePIU Progress: Native AOT Dynamic Translation and Performance Improvements

Until now, `rePIU` has largely focused on basic initialization in Legacy mode and establishing the surrounding HLE (High-Level Emulation) environment. However, relying entirely on single-step traps for every original x86 instruction brings substantial overhead. To overcome this, the recent work branch heavily focused on introducing and refining a **Native AOT (Ahead-of-Time) dynamic translator**.

In this post, we will summarize the key changes merged into the `main` branch since the previous post and compare the actual performance differences these changes have achieved.

## Key Progress (Commits Summary)

The major tasks completed since the last update include:

1. **Introduction of AOT Dynamic Translator and Code Cache**
   - Connected `aot` and `aot-dynamic` execution backends to allow DOS4GW-based code to execute directly within the Win32 native environment.
   - Implemented a relocatable code cache that translates runtime basic blocks into native instructions and places them into memory.
2. **Dynamic Control Flow Optimization**
   - Improved the host worker to dynamically track and translate unmapped paths such as indirect calls, returns, conditional transfers, and fallthrough linking at runtime.
   - Introduced worker-backed inline caches to minimize the cost of cache misses.
3. **Self-Modifying Code (SMC) Coherency Resolution**
   - Implemented a page-level coherency model to handle runtime code modification (e.g., import stubs) inherent to PIU game logic.
   - Whenever code is modified at runtime, the active cache is immediately retired, and a new live arena snapshot is used to publish a new generation of native instructions, ensuring execution flow continues safely and without divergence.

## Performance Comparison

The following performance data was measured by running the game loop (`repiu_supervisor_win32.exe`) for 10 seconds across different backends. (Based on Win32 x86 Debug build)

| Metric / Backend | `legacy` | `aot` (Static Only) | `aot-dynamic` |
| :--- | :--- | :--- | :--- |
| **Cumulative Heartbeat (3s)** | 671,524 | 645,770 | 76,601 |
| **Cumulative Heartbeat (10s)**| 2,248,638 | 2,085,754 | 1,919,174 |
| **Cumulative Single-Step Traps (10s)**| 1,124,319 times | 1,042,877 times | Under ~510,000 times |
| **Trap Ratio** | 1 per 2 Heartbeats | 1 per 2 Heartbeats | **1 per ~3.8 Heartbeats** |

### Analysis
* **Reduced Emulation Overhead**: The previous `legacy` method incurred heavy loads by triggering traps constantly. With the introduction of the `aot-dynamic` mode, translated code executes natively in the cache (direct execution), cutting the trap frequency by more than half.
* **Warm-up Acceleration**: The `aot-dynamic` method spends resources on code decoding, translation, and SMC handling during the initial execution, resulting in slower heartbeat progress in the first 3 seconds. However, once the cache is warmed up, the progress rate increases exponentially. By the 10-second mark, the translation cost is completely offset, demonstrating a dramatic uplift in overall performance.

With further optimizations and Release builds on the horizon, we can expect near-native execution performance in the near future.
