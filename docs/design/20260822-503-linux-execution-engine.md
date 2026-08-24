# Linux 실행 엔진 이식 설계 (Stage 3)

## 배경

[Stage 1](20260822-501-linux-core-build.md)에서 공용 코어가, [Stage 2](20260822-502-linux-launcher.md)에서
런처가 i386 Linux로 넘어갔습니다. 남은 것은 `src/platform/win32`의 76개 소스, 즉 실행
엔진입니다.

## 조사 결과 — 예상이 두 번 빗나갔습니다

### 정정 1: SMC 감지에 Windows 전용 API를 쓰지 않습니다

Stage 1·2에서 저는 "SMC 감지가 Windows write-watch에 의존하므로 Linux 대응을 새로
설계해야 한다"고 여러 번 적었습니다. **틀렸습니다.** 자료구조 이름이
`Win32AotPageWriteWatchSet`이라 `GetWriteWatch` API를 쓴다고 단정했는데, 구현은
그렇지 않습니다.

```cpp
VirtualProtect(page, kGuestPageSize, PAGE_EXECUTE_READ, &previous)
```

게스트 페이지를 읽기+실행으로 보호해 두고 **쓰기가 예외로 잡히는** 방식입니다. Linux에는
`mprotect(PROT_READ|PROT_EXEC)` + `SIGSEGV`가 그대로 있습니다. 개념이 1:1로 대응하므로
재설계 대상이 아닙니다.

교훈은 Stage 1에서 probe 의존성을 grep으로 판정했다 틀린 것과 같습니다 — **이름이 아니라
구현을 봐야 합니다.**

### 정정 2: 게스트 `INT`는 접근 위반으로 도착합니다

boundary opcode census에서 `CD`(INT)가 297,310건으로 압도적인데, Windows는 이를
**`EXCEPTION_ACCESS_VIOLATION`** 으로 전달하고 코드가 EIP의 바이트를 디코드해
`CD 21`·`CD 2F` 등을 가려냅니다. Linux에서 `int $0x21`은 **`SIGSEGV`(일반 보호 예외)** 로
도착하므로 **같은 모양**입니다. 시그널을 받고 EIP를 디코드하는 흐름이 그대로 유지됩니다.

### 남은 실제 격차: 하드웨어 디버그 레지스터

`Dr0`~`Dr3`, `Dr6`, `Dr7`을 55곳에서 씁니다. **Linux 사용자 공간에서는 자기 스레드의 디버그
레지스터를 직접 쓸 수 없습니다**(ptrace로 다른 프로세스가 설정해야 합니다). 다만 이
용도는 `native_linear_span`·`native_fast_path`의 선택적 성능 실험이고
`REPIU_NATIVE_LINEAR_SPAN`은 **기본 꺼짐**입니다. Linux 초기 이식에서는 이 기능을
비활성으로 두고, 필요해지면 경계에 `INT3`을 임시로 심는 방식으로 대체합니다 — 프로젝트가
이미 여러 곳에서 쓰는 기법입니다.

## 예외 → 시그널 매핑

| Windows | Linux | 판별 |
|---|---|---|
| `EXCEPTION_ACCESS_VIOLATION` (게스트 `INT`) | `SIGSEGV` | EIP의 `CD xx` 디코드 (양쪽 동일) |
| `EXCEPTION_ACCESS_VIOLATION` (SMC 쓰기) | `SIGSEGV`, `si_code=SEGV_ACCERR` | `si_addr`가 보호된 게스트 페이지 |
| `EXCEPTION_SINGLE_STEP` | `SIGTRAP`, `si_code=TRAP_TRACE` | EFLAGS TF |
| `EXCEPTION_BREAKPOINT` (`INT3`) | `SIGTRAP`, `si_code=TRAP_BRKPT` | — |
| 접근 종류(읽기/쓰기) `ExceptionInformation[0]` | `mcontext.gregs[REG_ERR]`의 쓰기 비트 | — |
| 폴트 주소 `ExceptionInformation[1]` | `siginfo.si_addr` | — |

## 결정 1: 레지스터 컨텍스트는 이름을 유지한 채 타입만 바꿉니다 (3a)

`CONTEXT` 필드 사용은 328(`Eip`)·191(`Eax`)·157(`EFlags`)처럼 **표준 i386 레지스터 집합에
집중**돼 있습니다. 270곳을 일일이 새 API로 고치는 대신, **필드 이름을 그대로 두고 타입만
플랫폼별로 정의**합니다.

```
Windows : using GuestCpuContext = CONTEXT;            // 별칭, 무변화
Linux   : struct GuestCpuContext { uint32 Eip, Esp, Eax, ... };
```

Linux 구조체는 Windows와 **같은 필드 이름**(`Eip`, `Esp`, `EFlags`, `SegCs` …)을 갖고,
시그널 핸들러가 진입·복귀에서 `mcontext_t`와 변환합니다. 그러면 기존 328+191+157곳의
필드 접근이 **양쪽에서 그대로 컴파일**됩니다.

이 선택의 값은 위험 감소입니다. 270곳을 손으로 고치면 그 자체가 회귀 원인이 되지만,
타입 별칭은 Windows 쪽 생성 코드가 한 바이트도 바뀌지 않습니다.

디버그 레지스터 필드는 구조체에 두되 Linux에서는 항상 0이며, 이를 읽는 기능은 Linux에서
비활성입니다.

**3a 구현 중 정정:** 위 조사는 범용 레지스터만 세었습니다. `CONTEXT`의 모든 멤버로 다시 세니
둘이 더 있었습니다. `ContextFlags`(7곳)는 `GetThreadContext` 계열에 넘기는 Windows API
인자라 Linux에는 대응 개념이 없어 필드만 두고 무시합니다. `FloatSave`(5곳)는 게스트 x87
상태를 실제로 읽고 쓰므로 변환합니다 — glibc `_libc_fpstate`가 FSAVE 이미지 그대로라
Windows의 `FLOATING_SAVE_AREA`와 필드 대 필드로 대응합니다.

## 결정 2: 메모리 API는 얇은 계층으로 감쌉니다 (3b)

`VirtualProtect` 47곳, `VirtualQuery` 15곳, `VirtualAlloc` 8곳입니다.

* `VirtualProtect` → `mprotect`. 보호 상수는 자체 열거형으로.
* `VirtualQuery` → 이건 단순 대응이 없습니다. Linux는 `/proc/self/maps`를 읽어야 하고
  비용이 다릅니다. 용도가 "이 주소가 커밋되어 있는가"에 가까우므로, **질문 자체를 좁혀**
  필요한 정보만 제공하는 함수로 감쌉니다.
* `VirtualAlloc` → `mmap`. 고정 주소 예약은 `MAP_FIXED_NOREPLACE`가 대응합니다.

**3b 구현 중 정정 셋.** 위 숫자는 `VirtualFree`(30곳)를 빠뜨렸고 나머지도 낮았습니다 —
실제는 `VirtualProtect` 50, `VirtualFree` 30, `VirtualAlloc` 16, `VirtualQuery` 16, 합 112곳입니다.

첫째, **`PAGE_GUARD`·`PAGE_WRITECOPY`·`PAGE_NOACCESS`를 `VirtualProtect`에 넘기는 곳은
하나도 없습니다.** 이 상수들은 오직 "이 보호값이 읽기 가능한가"를 판정하는 **손으로 쓴 분류기
다섯 개** 안에만 나오고, 그 다섯이 서로 조금씩 다릅니다. 그래서 비트마스크를 그대로 노출하는
대신 `MemoryRegion`이 `readable`·`writable`·`executable`로 **답을 직접 줍니다**. 중복 다섯
개가 하나로 줄어듭니다.

둘째, **`VirtualProtect`가 돌려주는 이전 보호값은 실제로 소비됩니다.** 게스트 스토어 경로가
페이지를 쓰기 가능으로 바꾸고, 쓰고, **이전 값으로 되돌립니다**. `mprotect`는 이전 값을 알려
주지 않으므로 Linux 백엔드는 자기가 설정한 것을 **그림자 테이블**에 기록해 돌려줍니다.
`/proc/self/maps`를 게스트 스토어마다 파싱하는 것은 불가능하기 때문입니다.

셋째, `VirtualQuery`의 용도는 "커밋되어 있는가" 하나가 아니라 넷입니다 — 커밋 여부, 현재
보호값, 진단용 리전 경계, 그리고 **게스트 스택 한계를 얻는 `AllocationBase`**. 마지막 것은
보호 변경으로 리전이 쪼개져도 **예약 전체의 시작**을 가리켜야 하므로, 그림자 테이블은 보호
구간과 예약을 **따로** 추적합니다.

## 결정 3: 시그널 핸들러가 VEH의 자리를 대신합니다 (3c)

VEH는 프로세스 전역 등록이고 시그널은 스레드 단위 마스크를 갖지만, 실행 엔진은 게스트
스레드 하나에서만 예외를 처리하므로 차이가 문제되지 않습니다.

주의할 점 셋을 설계에 못박습니다.

* **`sigaltstack`이 필요합니다.** 게스트 스택이 손상됐거나 호스트 스택으로 전환 중일 때도
  핸들러가 돌아야 합니다. Windows는 VEH가 커널이 마련한 스택에서 돌아 이 문제가 없습니다.
* **핸들러에서 EIP를 고쳐 재개하는 것이 정상 경로입니다.** Windows의
  `EXCEPTION_CONTINUE_EXECUTION`에 해당하며, Linux에서는 `ucontext`를 수정하고 반환하면
  됩니다.
* **비동기 시그널 안전성은 적용되지 않습니다.** 이 핸들러는 동기 폴트(SIGSEGV/SIGTRAP)만
  처리하므로 일반 코드를 호출해도 됩니다. 다만 타이머 주입 같은 비동기 경로를 시그널로
  옮기면 그때는 규칙이 달라집니다.

## 결정 4: naked thunk는 GAS로 다시 씁니다 (3d)

`__declspec(naked)` + MSVC 인라인 어셈블리는 GCC에 없습니다. GCC는 `__attribute__((naked))`를
x86에서 지원하지 않으므로, 전역 어셈블리(`asm(".globl ...")`)로 옮깁니다.

이때 `fs:[4]`·`fs:[8]` 조작 28곳은 **대부분 사라질 것으로 봅니다.** Windows TIB의
StackBase/StackLimit를 호스트 스택으로 바꿔 쓰는 코드인데, Linux 커널은 스레드 스택
경계를 그런 구조체에서 읽지 않습니다. **가설이며 3d에서 확인합니다.**

**3d-1에서 확인됨.** 전환된 호스트 스택 위에서 폴트를 일으켜, 3c 핸들러가 전달하고 재개하는
것을 양쪽에서 같은 probe로 확인했습니다. Linux는 TIB에 아무것도 알리지 않고 통과합니다 —
`sigaltstack`이 그 자리를 대신하므로 28곳은 Linux 쪽 thunk에서 **전부 사라집니다.**

**3d-16에서 확장됨.** 다섯 thunk에 이어 트램폴린의 세 진입점 — 게스트로 들어가는 스택 전환
`CallGuestEntryWithStack`과 폴트 복귀 둘 — 도 GAS로 옮겼습니다. TIB 결론은 그대로
적용됩니다. 두 가지가 새로 갈립니다.

* **`fxsave`는 여기에 없습니다.** 3d-12는 다섯 thunk가 Linux에서 FPU를 저장해야 한다고
  판정했고, 근거는 GCC가 i386에서 x87로 계산하므로 **호스트 resolver**가 게스트의 x87 스택을
  밀어낸다는 것이었습니다. 이 진입점이 부르는 것은 호스트 코드가 아니라 게스트이고, i386
  SysV ABI는 호출 경계에서 x87 스택이 비어 있을 것을 요구하므로 건너갈 호스트 상태 자체가
  없습니다.
* **세그먼트 복원의 무게가 반대입니다.** Windows에서 `RecoverGuestStackException`의
  `mov ds, ax` 계열은 폴트 핸들러가 컨텍스트에도 같은 값을 쓰므로 이중 안전장치입니다.
  Linux에서는 3a가 세그먼트 레지스터를 되쓰지 않기로 명시했으므로 **이 코드가 유일한 복원
  경로**입니다.

오프셋은 `include/repiu/platform/guest_stack_switch.h`에 `#define`으로 한 번만 둡니다. `.S`가
C 전처리기를 거치므로 어셈블리와 C++ `static_assert`가 같은 숫자를 읽고, 두 구현이 필드
배치에서 갈릴 수 없습니다.

## 결정 5: 호스트 키 폴링은 SDL 키보드 상태로 옮깁니다 (3d)

`GetAsyncKeyState`는 세 곳에서 쓰이고, 셋 다 묻는 것이 같습니다 — "지금 이 키가 눌려 있는가".
`0x0001`("지난 호출 이후 눌림") 비트는 스냅샷의 의미를 바꾸기 때문에 **의도적으로 쓰이지
않습니다.** 그래서 대응물은 하나로 정해집니다: `SDL_GetKeyboardState()`가 돌려주는,
`SDL_Scancode`로 인덱싱하는 배열입니다.

비용은 오히려 내려갑니다. Task 403이 `GetAsyncKeyState`를 포트 I/O 핸들러 본체의 99.21%로
지목했는데, **키마다 한 번씩 부르던 호출이 배열 인덱싱**이 되기 때문입니다. `SDL_GetModState()`
는 `ReadWin32ModifierState`의 여섯 번 호출을 한 번으로 줄입니다.

달라지는 것이 셋 있고, 그중 둘만 실제 차이입니다.

| 성질 | `GetAsyncKeyState` | `SDL_GetKeyboardState` | 판단 |
|---|---|---|---|
| 포커스 | 전역 | 창이 받은 키만 | 사용자 확인: 이 프로젝트에 무관 |
| 신선도 | OS의 실시간 상태 | `SDL_PumpEvents`가 갱신 | Task 403의 스냅샷 경계가 이미 더 성김 |
| 레이아웃 해석 시점 | 읽을 때(VK가 레이아웃 매핑) | resolve할 때 | resolve 전에 SDL 비디오를 올림 |

세 번째가 이 결정에서 유일하게 설계를 요구하는 지점입니다. 지금은 `SDLK_M` → `VK_M`이
고정 변환이고 레이아웃은 `GetAsyncKeyState`가 읽을 때 반영합니다. 앞으로는
`SDL_GetScancodeFromKey`가 **현재 레이아웃 기준으로** 스캔코드를 정하므로, 해석이 load
시점으로 옮겨갑니다.

**처음에는 `SDL_EVENT_KEYMAP_CHANGED`에서 다시 resolve하는 것으로 잡았다가 물렀습니다.**
`active_jamma_bindings.h`가 "시작 시점에 한 번만 쓰고 그 뒤로는 read-only"를 명시하고,
그것이 게스트 스레드와 호스트 스레드가 락 없이 읽는 근거이기 때문입니다. keymap 이벤트는
그 "시작 이후"에 해당하므로, 거기서 쓰면 게스트가 폴링하며 읽는 필드를 호스트 스레드가
고치게 됩니다. 필드를 원자적으로 만들거나 이중 버퍼로 바꾸는 길은 3d-3이 `Interlocked*`에서
마주친 것과 같은 자리 — **가장 뜨거운 경로의 자료 구조를 위해 계약을 바꾸는 일**이라
택하지 않았습니다.

대신 반대편에서 잡습니다. resolve 시점에 SDL이 레이아웃을 알고 있으면 되므로, 바인딩을
읽기 전에 `SDL_InitSubSystem(SDL_INIT_VIDEO)`를 부릅니다 — keymap은 비디오 서브시스템이
올라올 때 생기고, 없으면 `SDL_GetScancodeFromKey`가 SDL 기본 레이아웃으로 답합니다. 참조
카운트 방식이라 렌더 백엔드는 나중에 자기 몫으로 다시 초기화합니다. 남는 차이는 **실행 중
레이아웃을 바꾸면 다시 시작해야 한다**는 것 하나이고, 기본 바인딩의 P1이 문자 키
(`Q, E, S, Z, C`)라 이 순서는 설정 파일이 없는 실행에도 해당합니다.

스레드 관점도 맞습니다. `SDL_GetKeyboardState`와 `SDL_GetModState`는 SDL 헤더가 **"any
thread"로 명시**하므로 게스트 스레드의 스캔 경로에서 그대로 부를 수 있고, 반환 포인터는
프로세스 수명 동안 유효한 내부 배열을 가리킵니다. 스레드 안전하지 않은
`SDL_GetScancodeFromKey`는 load 시점에만 부릅니다 — 스캔 경로는 변환하지 않는다는 Task 403의
제약이 그대로 지켜집니다.

결과적으로 `HostKeyAlias::virtual_key`는 `scancode`가 되고, VK 변환표 90줄과
`win32_host_key_translation`이 통째로 사라집니다. resolve는 `repiu::input`으로 올라갑니다 —
Win32 개념이 빠지고 나면 그것은 더 이상 플랫폼 코드가 아니기 때문입니다.

## 결정 6: 환경 블록 열거는 계층으로 내리고 Windows는 프로세스 환경을 유지합니다 (3d-16)

3d-9는 환경 **변수 하나**를 읽는 `ReadEnvironmentSetting`을 헤더 전용 inline으로 뒀습니다.
게스트에게 넘기는 DOS 환경 블록은 **전부**를 복사하므로 그것으로는 답이 되지 않습니다.

`ForEachEnvironmentEntry`는 선언만 헤더에 두고 구현을 양쪽 백엔드에 둡니다. 헤더에 정의를 둘
수 없는 이유는 Windows 구현이 `GetEnvironmentStringsA`를 쓰기 때문이고, 그것은 3d-2가
헤더에서 걷어낸 `windows.h`를 도로 부릅니다.

MSVC의 `_environ`을 쓰면 헤더 전용으로 남길 수 있지만 택하지 않았습니다. `_environ`은 CRT가
시작 시점에 만든 사본이라 이후의 `SetEnvironmentVariable`을 보지 못합니다. 두 호스트를 한
구현으로 맞추기 위해 Windows 동작을 바꾸는 것은 이 이식의 규칙을 거스릅니다.

**3d-17에서 쓰기 쪽이 붙으면서 비대칭이 드러났습니다.** Windows에서 이 계층은 **읽기를 두
군데에서** 합니다 — `ReadEnvironmentSetting`은 `std::getenv`로 CRT 사본을,
`ForEachEnvironmentEntry`는 `GetEnvironmentStringsA`로 프로세스 블록을 읽습니다. 그래서 쓰기
백엔드는 `SetEnvironmentVariableA`가 될 수 없습니다. 그것은 프로세스 블록만 갱신하므로 런처가
심은 값을 엔진의 `ReadEnvironmentSetting` 호출부 열일곱 곳이 못 봅니다. `_putenv_s`가 둘 다
갱신하고, 그래서 런처가 원래부터 그것을 쓰고 있었습니다. 우연히 맞은 것이 아니라 **`_putenv_s`가
유일한 답이라서** 맞습니다. POSIX에는 이 갈래가 없습니다 — `setenv`가 `environ`을 갱신하고
`getenv`와 열거가 같은 것을 읽습니다.

## 결정 7: 엔진은 목록 하나로 링크하고, 자식 프로세스의 근거는 미측정으로 남깁니다 (3d-17)

3d-16이 "남은 것은 링크"라고 적었고, 3d-17은 그것이 무엇을 요구하는지 먼저 쟀습니다. 엔진
소스 80개를 오브젝트로 만들어 링크하면 미정의 심볼이 아홉 개인데 **엔진 수준은 하나도
없습니다** — `-lGL` 셋과 miniz의 deflate 아홉이 전부이고, 둘 다 라이브러리 배선 문제입니다.

그래서 소스 목록을 **하나로** 둡니다. Linux 대응물이 있는 다섯 파일은 Linux에서 빈 오브젝트가
되지만, 목록을 둘로 쪼개면 새 파일마다 "어느 쪽인가"를 묻게 되고 그 답은 이미 파일 안의
울타리에 있습니다.

`INFINITE`는 규약이 아니라 표기만 옮겼습니다. 로더가 엔진에 넘기던 이 값은 대기의 타임아웃으로
끝나므로 Windows 상수 이름이 붙어 있었는데, 그 값이 **엔진 API를 건너가는** 이상 그 이름은 한쪽
호스트에만 있는 이름입니다. 중립 상수를 두고 Windows 대기 옆에 `static_assert`로 같음을
고정했습니다 — 숫자가 같으므로 동작은 그대로이고, 같다는 사실이 주석이 아니라 컴파일러의
단정이 됩니다.

**자식 프로세스 재실행은 옮기되 근거를 확인하지 않았습니다.** Task 500이 이 구조를 만든 이유는
GPU 드라이버가 게스트 주소 공간을 선점하기 때문인데, Linux에서도 같은지는 **측정하지
않았습니다.** Task 502가 "실행 엔진이 생긴 뒤에 판단"으로 미뤘고, 엔진이 링크되는 지금도 아직
돌려본 적이 없어 판단할 근거가 없습니다. 동작만 같게 두고(`posix_spawn`), 근거가 미측정임을
여기에 적어 둡니다 — Linux가 이 우회를 필요로 하지 않는 것으로 밝혀지면 되돌릴 자리가
`host_process.h`입니다.

## 결정 8: 스레드 계층은 `running`을 따로 답하고, `TerminateThread`는 대응물을 만들지 않습니다 (3d-18)

3d-14가 kernel32 스레드 테이블을 울타리에 넣으며 남긴 질문 둘에 답합니다.

첫째, **`STILL_ACTIVE`를 노출하지 않습니다.** Windows의 `GetExitCodeThread`는 도는 스레드에
259를 돌려주는데 259는 적법한 종료 코드이기도 합니다. 엔진의 두 호출부가 `!= STILL_ACTIVE`로
판정하고 있었으므로, 게스트가 259로 끝나면 폴 루프는 타임아웃까지 계속 돌았을 것입니다.
`HostThreadStatus`는 `running`과 `exit_code`를 따로 답하고, Windows 백엔드는 길이 0의 대기로
먼저 갈라 모호함을 없앱니다. 3b가 보호 비트마스크 대신 `readable`을 돌려준 것과 같은 자리입니다.

**오늘 이 값이 나오지는 않습니다** — 스레드 프로시저는 0·1·2·4·5를, 감시견은 0이나 3을
씁니다. 잠재적 결함이지 살아 있는 결함이 아닙니다.

둘째, **`TerminateThread`에는 대응물을 만들지 않습니다.** 호출부를 읽으면 그것은 최후 수단이고,
그 앞에 이미 우아한 경로가 있습니다 — 스레드를 정지시키고 `RecoverToHost`로 컨텍스트를 복귀
진입점으로 돌린 뒤 재개하는 것입니다. 3d-16이 Linux에서 그 기제를 이미 세웠고 probe로
확인했으므로, Linux는 시그널로 같은 일을 할 수 있습니다. 즉 Linux에서는 **우아한 경로가 유일한
경로**이며, `pthread_cancel`은 답이 아닙니다 — 취소 지점에서만 작동하는데 게스트는 거기에
도달하지 않습니다. 실제 인터럽트 구현은 폴 루프와 함께 3d-19의 일입니다.

## 단계 구분

| 하위 단계 | 내용 | Linux 실행 경로 |
|---|---|---|
| **3a** | 레지스터 컨텍스트 추상화 + 변환 함수 + probe | 없음 (순수 리팩터링) |
| 3b | 메모리 API 추상화 | 없음 |
| 3c | 시그널 기반 예외 핸들러 | **처음 생김** |
| 3d | thunk 재작성, 실제 구동 | 게임 실행 시도 |

3a와 3b에서 확인해야 할 첫 번째 것은 **Windows 회귀 없음**입니다. 게스트를 실행하는
경로가 아직 Linux에 없으므로 대부분은 "통과했다"가 아니라 "아무것도 바뀌지 않았다"를
확인하는 단계입니다. 다만 3a의 `ucontext_t` 변환만은 예외로, 실제로 Linux에서 돌려볼 수
있으므로 probe로 왕복 검증합니다.

## 범위 밖

* Wayland 백엔드, 발판 입력, 인게임 OSD
* 하드웨어 디버그 레지스터를 쓰는 성능 실험(`native_linear_span`)의 Linux 대응
* CHD 마운트와 자산 경로의 Linux 검증

---

# Linux Execution Engine Port Design (Stage 3)

## Background

Stage 1 moved the neutral core to i386 Linux and Stage 2 the launcher. What remains is the
execution engine: the 76 sources under `src/platform/win32`.

## Two expectations the investigation overturned

**Self-modifying-code detection uses no Windows-specific API.** Stages 1 and 2 repeatedly recorded
that it depended on Windows write-watch and would need redesigning. That was wrong, inferred from
the name `Win32AotPageWriteWatchSet` rather than from the implementation, which protects guest
pages with `VirtualProtect(PAGE_EXECUTE_READ)` and catches the write as an exception. Linux has
exactly that in `mprotect(PROT_READ|PROT_EXEC)` plus `SIGSEGV`. The lesson repeats Stage 1's: read
the implementation, not the name.

**Guest `INT` instructions arrive as access violations.** `CD` dominates the boundary opcode census
at 297,310 occurrences, and Windows delivers those as `EXCEPTION_ACCESS_VIOLATION`, after which the
code decodes the bytes at EIP to recognise `CD 21`, `CD 2F`, and the rest. On Linux `int $0x21`
raises `SIGSEGV`, which is the same shape: take the signal, decode at EIP.

**The real gap is the hardware debug registers.** `Dr0`-`Dr3`, `Dr6`, and `Dr7` appear at 55 sites,
and Linux user space cannot write its own thread's debug registers. They serve `native_linear_span`
and `native_fast_path`, an opt-in performance experiment that is off by default, so Linux disables
that feature initially and can later plant a temporary `INT3` at the boundary instead — a technique
already used elsewhere in the project.

## Decisions

**The register context keeps its field names and changes only its type.** Field use concentrates in
the standard i386 set — `Eip` 328 times, `Eax` 191, `EFlags` 157 — so instead of rewriting 270 call
sites, `GuestCpuContext` aliases `CONTEXT` on Windows and is a struct with the same field names on
Linux, converted to and from `mcontext_t` at signal entry and exit. Every existing field access then
compiles unchanged on both systems. The value here is risk: editing 270 sites by hand would itself
be a source of regressions, while an alias leaves the Windows build byte-identical. A correction from
implementing 3a: that count covered only the general registers, and recounting against every
`CONTEXT` member found two more. `ContextFlags`, at 7 sites, is an argument to `GetThreadContext`
and its relatives with no Linux counterpart, so the field exists and is ignored; `FloatSave`, at 5,
holds real guest x87 state and is converted, which is direct because glibc's `_libc_fpstate` is the
same FSAVE image as Windows' `FLOATING_SAVE_AREA`.

**The memory API gets a thin layer.** `VirtualProtect` maps to `mprotect` and `VirtualAlloc` to
`mmap`, with `MAP_FIXED_NOREPLACE` for fixed reservations. `VirtualQuery` has no simple counterpart
— Linux would mean parsing `/proc/self/maps` at a very different cost — so the wrapper narrows the
question to what the callers actually ask. Three corrections from implementing 3b: those counts
omitted `VirtualFree` entirely and understated the rest, the real figures being 50 `VirtualProtect`,
30 `VirtualFree`, 16 `VirtualAlloc`, and 16 `VirtualQuery`, 112 sites in all. No caller ever passes
`PAGE_GUARD`, `PAGE_WRITECOPY`, or `PAGE_NOACCESS` to `VirtualProtect` — those appear only inside
five separately hand-written readability classifiers that disagree in their details, so the region
structure answers readability directly and the five collapse into one. The previous protection that
`VirtualProtect` reports *is* consumed, by the guest store path that protects a page writable,
writes, and restores it, so the Linux backend records what it sets in a shadow table rather than
parsing `/proc/self/maps` on every guest store. And `VirtualQuery` answers four questions, not one:
commit state, current protection, region bounds for diagnostics, and the allocation base the guest
stack limit is derived from — which must keep naming the whole reservation even after a protection
change splits it, so reservations are tracked separately from protection intervals.

**Signal handlers take the vectored handler's place.** A vectored handler is process-wide and
signals are per-thread, but only the guest thread faults, so the difference does not bite. Three
things are fixed by design: `sigaltstack` is required, because the handler must run even when the
guest stack is damaged or being switched, a problem Windows does not have; rewriting the resumption
address inside the handler is the normal path, corresponding to `EXCEPTION_CONTINUE_EXECUTION`; and
async-signal-safety does not apply, because these are synchronous faults — though it would apply if
timer injection later moved onto a signal.

**Naked thunks are rewritten in GAS.** MSVC's `__declspec(naked)` with inline assembly has no GCC
equivalent on x86, so those move to global assembly. The 28 `fs:[4]`/`fs:[8]` sites are expected to
disappear, since they exist to keep the Windows TIB's stack bounds consistent and the Linux kernel
does not consult such a structure — a hypothesis to confirm in 3d. **Confirmed in 3d-1**: one probe,
run on both hosts, switches to the host stack and takes a fault there, and the 3c handler delivers
and resumes on Linux with nothing having been told about stack bounds. `sigaltstack` takes that
role, so all 28 sites disappear from the Linux thunks. **Extended in 3d-16**: after the five thunks,
the trampoline's three entries follow — `CallGuestEntryWithStack`, which is the stack switch into the
guest, and the two fault recoveries. The TIB conclusion carries over unchanged; two things differ.
There is no `fxsave` here, because 3d-12's reason for adding one to the five was that GCC computes in
x87 on i386 and so a *host resolver* displaces the guest's x87 stack, whereas what this entry calls
is the guest, and the i386 System V ABI requires the x87 stack to be empty at a call boundary — there
is no host state crossing it. And the weight of the segment restores inverts: on Windows the
`mov ds, ax` sequence in `RecoverGuestStackException` is a second belt, since the fault handler also
writes those selectors into the context it resumes, while on Linux 3a deliberately does not write
segment registers back, which makes this code the only thing that restores them. The field offsets
are defined once, as `#define`s in `include/repiu/platform/guest_stack_switch.h`: a `.S` goes through
the C preprocessor, so the assembly and the C++ `static_assert`s read the same numbers and the two
implementations cannot disagree about the layout.

**Host key polling moves onto SDL's keyboard state.** `GetAsyncKeyState` appears at three sites and
all three ask the same question — is this key down now. The `0x0001` "pressed since last call" bit is
deliberately not consulted, because reading it would consume state and change what the snapshot
means, which settles the counterpart: the scancode-indexed array `SDL_GetKeyboardState()` returns.
The cost goes down rather than up, since Task 403 measured `GetAsyncKeyState` as 99.21% of the port
I/O handler's body and a per-key call becomes an array index; `SDL_GetModState()` likewise replaces
six calls with one. Three properties change and only two are real differences: focus, where SDL sees
only what the window received and the user has confirmed that is irrelevant here; freshness, where
the array is refreshed by `SDL_PumpEvents` rather than read live from the OS, a bound Task 403's
snapshot is already coarser than; and when the layout is resolved. That last one is the only part
needing design: today `SDLK_M` maps to `VK_M` by a fixed table and the layout is applied by
`GetAsyncKeyState` at read time, whereas `SDL_GetScancodeFromKey` picks the scancode against the
current layout at load time. Re-resolving on `SDL_EVENT_KEYMAP_CHANGED` was the first answer and was
withdrawn: `active_jamma_bindings.h` states that the set is written once at startup and read-only
afterwards, which is what lets the guest thread and the host thread read it with no lock, and a
keymap event is exactly "after startup". Making the fields atomic or double-buffering the set is the
place 3d-3 reached with `Interlocked*` -- changing a contract for a structure on the hottest path --
and was not taken. The layout is instead known by the time of the resolve: `SDL_InitSubSystem(SDL_INIT_VIDEO)`
runs before the bindings are read, since the keymap appears with the video subsystem and
`SDL_GetScancodeFromKey` answers from SDL's default layout without one. What remains is that changing
keyboard layout mid-run needs a restart, and the ordering matters even with no configuration file
because the built-in P1 defaults are letters. The threading also fits: SDL documents
`SDL_GetKeyboardState` and `SDL_GetModState` as safe from any thread, so the guest thread's scan path
may call them directly and the returned pointer names an internal array valid for the life of the
process, while `SDL_GetScancodeFromKey`, which is not thread safe, is called only at load. What
follows is that `HostKeyAlias::virtual_key` becomes `scancode`, ninety lines of virtual-key table and
`win32_host_key_translation` disappear, and the resolve rises into `repiu::input` — with the Win32
concept gone it is no longer platform code.

**Enumerating the environment moves into the layer, and Windows keeps asking the process.** 3d-9 put
`ReadEnvironmentSetting`, which reads *one* variable, in a header as an inline function. The DOS
environment block handed to the guest copies *all* of them, so that does not answer it.
`ForEachEnvironmentEntry` is therefore declared in the header and implemented in both backends — it
cannot be defined in the header because the Windows implementation uses `GetEnvironmentStringsA`,
which would bring back the `windows.h` 3d-2 spent a sub-stage removing. MSVC's `_environ` would have
kept it header-only and was not taken: it is a copy the CRT makes at startup and does not see a later
`SetEnvironmentVariable`, and changing Windows behaviour to let one implementation serve both hosts
is against this port's rule.

**3d-17 added the write half, and an asymmetry came out with it.** On Windows this layer *reads in two
places*: `ReadEnvironmentSetting` reads the CRT's copy through `std::getenv`, and
`ForEachEnvironmentEntry` reads the process block through `GetEnvironmentStringsA`. So the write
backend cannot be `SetEnvironmentVariableA`, which updates only the second — a value published that
way would be invisible to all seventeen `ReadEnvironmentSetting` call sites in the engine.
`_putenv_s` updates both, which is why the launcher already used it: right not by coincidence but
because it is the only answer. POSIX has no such split, since `setenv` updates `environ` and both
`getenv` and the enumeration read that.

**The engine links as one source list, and the child process keeps an unmeasured rationale.** 3d-16
said what remained was linking, and 3d-17 measured what that asks for first: building all 80 engine
sources into objects and linking them leaves nine undefined symbols and **none of the engine's own** —
three for `-lGL` and nine miniz deflate wrappers, both library wiring rather than porting. The source
list therefore stays single. The five files with Linux counterparts compile to empty objects there,
and splitting the list would make every new file a question whose answer already lives in the fence
inside it.

`INFINITE` changed spelling, not convention. The loader handed this value to the engine and it ends up
as a wait's timeout, which is why it carried a Windows constant's name — but a value that *crosses the
engine's API* cannot be named after something only one host has. A neutral constant holds it now,
pinned to `INFINITE` by a `static_assert` beside the Windows wait: the number is the same, so nothing
behaves differently, and their being the same is checked by the compiler rather than asserted in a
comment.

**The child-process relaunch was carried across without confirming its reason.** Task 500 built it
because a GPU driver claims the address space the guest needs; whether Linux behaves the same is
**not measured.** Task 502 deferred that until an execution engine existed, and with the engine
linking but never yet run there is still nothing to decide on. The behaviour is matched with
`posix_spawn` and the gap recorded here — if Linux turns out not to need the detour,
`host_process.h` is where it comes out.

**The thread layer answers `running` separately, and `TerminateThread` gets no counterpart.** This
settles the two questions 3d-14 left when it fenced the kernel32 thread table. First, `STILL_ACTIVE`
is not exposed: Windows reports 259 for a running thread and 259 is also a legal exit code, and both
of the engine's call sites decided with `!= STILL_ACTIVE`, so a guest that exited with it would have
kept the poll loop spinning until the timeout. `HostThreadStatus` answers `running` and `exit_code`
separately, and the Windows backend splits on a zero-length wait so the ambiguity disappears — the
same move 3b made when it returned `readable` instead of a protection bitmask. The value does not
occur today (the thread procedure returns 0, 1, 2, 4 or 5 and the watchdog passes 0 or 3), so this is
a latent defect rather than a live one. Second, `TerminateThread` is not something to find a
counterpart for. Reading the call site shows it is the last resort behind a graceful path that
suspends the thread, points its context at the recovery entry with `RecoverToHost`, and resumes —
a mechanism 3d-16 already stood up and probed on Linux, where a signal does the same work. On Linux
the graceful path is therefore the only path; `pthread_cancel` is not the answer, since it acts at
cancellation points the guest never reaches. The interrupt itself belongs to 3d-19, with the poll
loop.

## Sub-stages

3a abstracts the register context, 3b the memory API, 3c introduces the signal handler and with it
the first Linux execution path, and 3d rewrites the thunks and attempts a real run. For 3a and 3b the first thing to
verify is that Windows did not regress: with no path that runs the guest on Linux yet, the claim is
mostly not that something passed but that nothing changed. The one exception is 3a's `ucontext_t`
conversion, which can actually be exercised on Linux and so is round-tripped by a probe.

## Out of scope

The Wayland backend, panel input, the in-game OSD, a Linux counterpart for the debug-register
performance experiment, and verifying the CHD mount and asset paths on Linux.
