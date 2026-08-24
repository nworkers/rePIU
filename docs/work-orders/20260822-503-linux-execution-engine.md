# Linux 실행 엔진 이식 작업 지시 (Stage 3)

설계: [20260822-503-linux-execution-engine.md](../design/20260822-503-linux-execution-engine.md)

Stage 3은 하위 단계로 나뉩니다. 각 단계가 끝날 때마다 이 문서에 절을 덧붙입니다.

## 3a — 레지스터 컨텍스트 추상화

1. `include/repiu/platform/guest_cpu_context.h`를 추가합니다. Windows에서는
   `using GuestCpuContext = CONTEXT;`, 그 외에서는 **같은 필드 이름**을 가진 구조체입니다.
   필드 이름을 바꾸지 않는 것이 이 단계의 전부입니다 — 270곳을 손대는 순간 이 이식의
   가치인 "Windows 동작 불변"이 사라집니다.
2. 디버그 레지스터 필드(`Dr0`~`Dr3`, `Dr6`, `Dr7`)를 구조체에 두되 Linux에서는 항상 0으로
   둡니다. 이 필드를 언급하는 코드가 양쪽에서 컴파일되어야 하기 때문입니다.
3. `src/platform/linux/guest_cpu_context.cpp`에 `ucontext_t` 변환을 구현합니다.
   `LoadGuestCpuContext`, `StoreGuestCpuContext`, 그리고 폴트 주소·접근 방향을 뽑는
   `ReadGuestFaultInfo`입니다. i386이 아닌 빌드는 조용히 잘못된 레이아웃을 읽는 대신
   `false`를 돌려줍니다.
4. `guest_cpu_context` probe를 `repiu_core_probe`에 추가합니다. Linux에서는 왕복 변환과
   폴트 정보 추출을, Windows에서는 필드 집합의 존재를 확인합니다.
5. Windows Debug에서 `repiu`와 `repiu_core_probe`를 다시 빌드해 회귀가 없음을 확인합니다.
6. Linux i386에서 `repiu_core_probe`를 빌드해 전부 통과하는지 확인합니다.

### 완료 조건

Windows 빌드와 probe 결과가 이전과 동일하고, Linux i386 `repiu_core_probe`가 새 probe를
포함해 전부 통과해야 합니다. 이 단계에서 Linux 실행 경로는 생기지 않습니다.

## 3b — 메모리 API 추상화

1. `Virtual*` 호출부 112곳을 **전부 읽고** 각각이 실제로 무엇을 묻는지 적습니다. 3a에서
   필드 개수를 세어 설계 누락을 찾았듯이, API 모양은 추측이 아니라 이 목록에서 나와야 합니다.
2. `include/repiu/platform/virtual_memory.h`에 `MemoryProtection` 열거형,
   `MemoryRegion`, `MemoryReservation`과 여섯 함수를 둡니다. 보호 비트마스크를 노출하지 말고
   `readable`·`writable`·`executable`로 **답을 주십시오** — 지금 코드에는 같은 판정을 하는
   손으로 쓴 분류기가 다섯 개 있고 서로 다릅니다.
3. `src/platform/win32/virtual_memory_win32.cpp`와 `src/platform/linux/virtual_memory.cpp`
   양쪽을 구현합니다. Windows 백엔드도 만드는 이유는, 그래야 **같은 probe로 두 구현을 대조**할
   수 있기 때문입니다.
4. Linux는 `mprotect`가 이전 보호값을 알려주지 않으므로 그림자 테이블에 기록합니다. 예약과
   보호 구간을 따로 추적해야 `allocation_base`가 보호 변경에 쪼개지지 않습니다.
5. `virtual_memory` probe를 `repiu_core_probe`에 추가합니다. **소스 하나가 양쪽에서 돌며 같은
   단정을 요구**해야 합니다. 두 구현을 따로 검사하는 probe는 이 단계의 목적을 못 채웁니다.
6. 양쪽에서 빌드하고 실행해 전부 통과시킵니다. Windows 회귀가 없어야 합니다.

### 완료 조건

같은 probe 소스가 Windows와 Linux i386에서 모두 통과해야 합니다. 두 호스트가 갈리는 지점이
발견되면 probe를 약화시키지 말고 **API가 그 차이를 흡수**하도록 고칩니다. 호출부 이전은 이
단계의 완료 조건이 아닙니다.

## 3c — 시그널 기반 폴트 핸들러

1. `include/repiu/platform/fault_handler.h`에 `FaultKind`, `FaultEvent`,
   `FaultDisposition`, 그리고 설치·해제 두 함수를 둡니다. 콜백은 함수 포인터 + `void*`로 —
   핸들러 안에서 할당이 일어나면 안 됩니다.
2. Windows는 VEH로, Linux는 `sigaction`으로 구현합니다. Windows에서 `GuestCpuContext`는
   `CONTEXT`이므로 **커널이 준 구조체를 콜백이 직접 편집**합니다(복사 없음). Linux는 3a의
   변환으로 읽고, 재개할 때 다시 씁니다.
3. Linux는 `sigaltstack`이 필수입니다. 게스트 스택이 손상됐거나 전환 중일 때도 핸들러가
   돌아야 합니다. `SA_NODEFER`도 답니다 — 엔진은 핸들러 안에서 브레이크포인트를 심고 단일
   스텝을 걸므로 중첩이 정상입니다.
4. `SIGSEGV`·`SIGBUS`·`SIGTRAP`·`SIGILL`·`SIGFPE`를 잡고 `si_code`로 분류합니다. `int3`가
   `TRAP_BRKPT`로 올지 `SI_KERNEL`로 올지는 커널에 따라 다르므로 **`TRAP_TRACE`가 아니면
   브레이크포인트**로 봅니다.
5. `fault_handler` probe를 추가합니다. **실제로 폴트를 일으켜** 전달·분류·재개를 확인해야
   합니다 — 이 단계는 다른 방법으로 검증할 수 없습니다. 핸들러가 **항상 전진**하도록 쓰십시오.
   재개하면서 아무것도 바꾸지 않으면 같은 명령을 영원히 반복합니다. **멈춘 probe는 실패한
   probe보다 훨씬 나쁩니다.**
6. 양쪽에서 빌드·실행해 전부 통과시키고 Windows 회귀가 없음을 확인합니다.

### 완료 조건

같은 probe 소스가 양쪽에서 통과해야 합니다. 최소한 읽기 폴트, 쓰기 폴트(방향 보고 포함),
그리고 **`int3` → 트랩 플래그 무장 → 단일 스텝 → 해제 → 정상 반환**의 왕복을 포함해야 합니다.
마지막 것이 엔진의 핵심 기제이므로 여기서 서면 3d가 설 자리가 없습니다.

## 3d-1 — 링크 방식과 스택 브리지

3d 전체(thunk 재작성 + 호출부 이전 + 실제 구동)는 한 단계로 다루기에 너무 큽니다. 남은
`src/platform/win32`는 78개 파일 약 42,000줄입니다. 먼저 **뒤따르는 모든 asm 작업의 전제를
정하는 부분**만 끊어냅니다.

1. Linux 바이너리의 링크 방식을 실측으로 정합니다. 손으로 쓴 thunk는 전역을 직접 주소로
   읽으므로 PIE에서는 텍스트 재배치가 생깁니다. **재배치가 생기면 텍스트 세그먼트가 쓰기
   가능해지고, 그건 페이지 보호로 SMC를 감지하는 이 엔진에 치명적**입니다. 반대로 평범한
   `-no-pie`는 i386 이미지를 0x08048000에 올리는데 그 주소는 게스트 재배치 범위
   0x01000000~0x09000000 **안**입니다. 두 제약을 모두 만족하는 설정을 찾아 근거와 함께
   남기십시오.
2. `src/platform/linux/stack_bridge.inc.S`에 브리지를 **매크로 하나로** 씁니다. 다섯 개
   디스패치 thunk는 부르는 resolver만 다르므로 복사본 다섯이 아니라 인스턴스 다섯이어야
   합니다. Intel 문법으로 써서 MSVC 원본과 줄 단위로 대조되게 하십시오 — thunk의 전사
   오류는 스스로 드러나지 않습니다.
3. `stack_bridge` probe를 추가합니다. **닮은 것을 새로 쓰지 말고 같은 매크로를 전개**해서
   시험하십시오. Windows에는 같은 계약을 MSVC asm으로 둡니다.
4. probe가 확인할 것: 프레임 배치(자기 정합), 스택 전환이 실제로 일어났는지, 프레임의 저장된
   EAX를 고치면 복귀 후 반영되는지, 컨텍스트가 없을 때 거부하는지, 그리고 호출자 스택이
   온전한지.
5. **`fs:[4]`·`fs:[8]` 가설을 증거로 바꾸십시오.** 설계는 이 28곳이 Linux에서 사라질 것으로
   봤습니다. 확인 방법은 하나뿐입니다 — 전환된 호스트 스택 위에서 **폴트를 일으켜** 3c
   핸들러가 여전히 전달·재개하는지 보는 것입니다. Windows에서 TIB를 바꾸는 이유가 바로 그
   상황이기 때문입니다.

### 완료 조건

같은 probe 소스가 양쪽에서 통과해야 합니다. Linux에서 TIB에 아무것도 알리지 않고 전환된
스택 위의 폴트가 처리되면 가설이 확인된 것이고, 그렇지 않으면 **가설이 틀린 것이므로 설계를
고쳐야 합니다.** Windows 회귀가 없어야 합니다.

## 3d-2 — 헤더에서 `windows.h` 걷어내기

3d-1의 측정이 말한 대로, 컴파일되지 않던 32개 중 24개가 헤더 하나에서 막혀 있습니다.
호출부를 3a·3b·3c의 API로 옮기는 작업의 첫 절반입니다.

1. `execution/thread_context.h`의 `<windows.h>`를 걷어냅니다. 그 헤더가 쓰는 것은 `HANDLE`
   3회뿐이고, Windows에서 `HANDLE`은 `void*`이므로 **같은 타입**입니다.
2. 그 include를 통해 transitively 받고 있던 헤더를 **전수로** 찾습니다. 하나를 고치면 다음이
   드러나는 식이라, 빌드를 반복하며 하나씩 쫓지 말고 목록부터 만드십시오.
3. `CONTEXT` → `repiu::platform::GuestCpuContext`, `EXCEPTION_POINTERS` → 태그 전방 선언.
   둘 다 Windows에서 **같은 타입으로 해석**되므로 정의도 호출부도 바뀌지 않아야 합니다.
4. 일괄 치환을 쓰되 **결과를 읽으십시오.** 이 저장소에는 이미 `using CONTEXT = _CONTEXT;`
   같은 전방 선언이 있고, 이름만 바꾸는 정규식은 그것을 문법 오류로 만듭니다. 주석 줄은
   건드리지 마십시오.
5. Windows 회귀를 확인합니다. `NOMINMAX`처럼 그 헤더를 통해 **우연히 얻고 있던 것**이 있다면
   드러날 것이고, 그건 원래 명시했어야 하는 것입니다.
6. Linux 측정을 다시 돌려 숫자가 얼마가 되는지 기록합니다.

### 완료 조건

Windows 빌드와 probe 결과가 이전과 같아야 하고, Linux에서 컴파일되는 소스 수가 46보다
늘어야 합니다. 남는 실패는 **왜 남는지** 분류해 3d-3의 범위로 넘깁니다.

## 3d-3 — 남은 Win32 API를 호출부에서 걷어내기

3d-2가 헤더 벽을 무너뜨렸고, 남은 26개는 전부 자기 소스에 진짜 Win32 API가 있는 파일입니다.
가장 큰 덩어리부터 처리합니다.

1. `Interlocked*`(152곳)를 먼저 **무엇에 걸리는지** 확인하십시오. `std::atomic`이 정답으로
   보이지만, 대상이 다른 프로세스가 매핑하는 고정 레이아웃이면 **감싸는 순간 그쪽이 읽는
   내용이 바뀝니다.** 필드 타입을 건드리지 말고 연산에만 이름을 붙이는 쪽이 맞을 수 있습니다.
2. 반환값 의미를 정확히 옮기십시오. `InterlockedIncrement`는 증가 **후**,
   `InterlockedExchange`는 저장 **전**을 돌려주고, 실제로 그 값을 쓰는 호출부가 있습니다.
3. 게스트 스토어 경로(`guest_memory_access.cpp`)를 3b API로 옮깁니다. 보호 해제 → 쓰기 →
   **이전 값 복원**이라는 패턴이 그대로 살아야 합니다.
4. `GetTickCount`는 호출부가 **차이만 쓰는지** 먼저 확인하십시오. 절대 시각을 읽는 곳이
   하나라도 있으면 교체할 수 없습니다. 32비트 폭은 유지하십시오 — 감김을 넘는 부호 없는
   뺄셈에 호출부가 의존합니다.
5. 타입 표기(`DWORD`, `SIZE_T`)를 옮길 때 **폭이 같다고 같은 타입이 아닙니다.** Windows에서
   `DWORD`는 `unsigned long`이고 `std::uint32_t`는 `unsigned int`라 포인터가 호환되지
   않습니다. 필드에서 파생시키십시오.
6. Windows 회귀를 확인하고 Linux 측정을 다시 돌립니다.

### 완료 조건

Windows 빌드와 probe가 이전과 같고, Linux에서 컴파일되는 소스가 52보다 늘어야 합니다.
남는 것은 **왜 남는지** 분류합니다.

## 3d-4 — 예외 디스패처를 `FaultEvent`로

남은 것의 중심입니다. 디스패처가 예외 구조체를 읽는 곳이 약 300곳이고 대부분
`execution_trampoline.cpp`(5,072줄) 안에 있으므로, 한 번에 하지 말고 **잎에서 뿌리로**
올라가십시오.

1. 먼저 각 핸들러가 예외 구조체에서 **실제로 무엇을 읽는지** 함수 단위로 세십시오. 전부
   `FaultEvent`에 있다면 이 단계는 시그니처 교체이고, 없는 것이 있다면 그것이 3c에 추가할
   것입니다.
2. `ExceptionAddress`는 Linux에 대응물이 **없습니다** — `si_addr`는 SIGSEGV에서 데이터
   주소입니다. `Eip`와 같은지 **측정하십시오.** 같다면 그 사용처 전부가 `Eip`로 끝나고,
   다르다면 경우마다 설계가 필요합니다. Windows 쪽 값은 예외 레코드에서 가져와 비교가
   동어반복이 되지 않게 하십시오.
3. 작은 핸들러 하나를 먼저 옮겨 패턴이 성립하는지 확인한 뒤 나머지를 일괄로 하십시오.
   레지스터가 이벤트에 있으므로 별도 컨텍스트 인자는 없어집니다.
4. 디스패처가 아직 Win32 구조체를 받는 동안에는 **한 곳에서** `FaultEvent`를 조립하십시오.
   호출부마다 조립하면 분류 로직 사본이 늘어납니다. 조립 함수는 3c 백엔드에 두어 분류가
   한 곳에만 있게 하고, **전이용임을 명시**하십시오.
5. 지역 변수를 없앨 때는 그 변수의 사용처를 함수 끝까지 확인하십시오. 400줄짜리 함수에서는
   눈으로 놓칩니다 — 컴파일러에 맡기되 빌드를 반드시 돌리십시오.
6. DBT 경로가 디스패처에 먹이려고 `EXCEPTION_RECORD`를 **합성**하는 곳이 있습니다.
   `FaultEvent`로는 합성할 것이 없어지므로 함께 정리하십시오.

### 완료 조건

Windows 빌드와 probe가 이전과 같고, Linux에서 컴파일되는 소스가 56보다 늘어야 합니다.
디스패처 본체의 시그니처 교체는 이 단계에 포함되지 않아도 됩니다 — 잎이 다 넘어오면
그때가 자연스러운 시점입니다.

## 3d-12 — 다섯 thunk를 GAS 매크로로 인스턴스화

3d-5부터 3d-11까지의 지시와 결과는 작업 로그에 이어서 남아 있습니다. 이 절은 3d-11이
"링크·구동으로 가는 마지막 asm 작업"으로 지목한 것을 다룹니다. 3d-11이 다섯
`aot_dbt_*_dispatch.cpp`를 Linux에서 컴파일되게 만들었지만, thunk 본체는 아직
`#if defined(_MSC_VER)` 안에 있어 Linux에서 주소가 `nullptr`입니다.

1. 다섯 원본 thunk를 3d-1의 매크로와 **줄 단위로 대조**하십시오. 3d-1은 "resolver 이름만
   다르다"고 기록했지만, 그것은 성공 경로만 본 것입니다. **컨텍스트가 없을 때의 경로를
   포함해 다시 세십시오.** 다른 것이 나오면 그것이 매크로의 매개변수가 되어야 합니다.
2. 거절 경로는 그냥 `ret`이면 안 됩니다. 디스패치 사이트는 자기 주소를 push하고 call하므로,
   되돌아갈 곳을 고치지 않고 반환하면 **사이트가 밀어 넣은 메타데이터로 뛰어듭니다.**
3. `src/platform/linux/aot_dbt_dispatch_thunks.S`에 다섯을 인스턴스화합니다. 각 `.cpp`는
   Linux에서 선언만 두고 `Get*ThunkAddress()`가 실제 심볼을 돌려주게 합니다.
4. `stack_bridge` probe에 **거절 경로를 추가**하십시오. 지금 probe는 성공 경로와 "컨텍스트가
   없으면 아무 일도 하지 않는다"만 봅니다. 네 thunk가 실제로 하는 것은 그것이 아닙니다.
   probe는 **사이트 모양의 호출자**를 두어야 합니다 — 주소를 push하고 call하는 것까지가
   계약의 일부이기 때문입니다.
5. 호출 규약도 probe에서 같게 하십시오. 3d-11이 다섯 resolver를 stdcall로 정했으므로,
   probe의 resolver가 cdecl이면 매크로는 **다른 ABI로 시험되는 것**입니다.
6. Windows 회귀를 확인하고, Linux에서 어셈블·빌드·probe 실행까지 확인합니다.

### 완료 조건

같은 probe 소스가 양쪽에서 통과해야 하고, 거절 경로가 **사이트가 밀어 넣은 주소에서 실제로
재개하는 것**을 보여야 합니다. Windows 빌드와 probe 결과가 이전과 같아야 합니다. 다섯 thunk가
Linux에서 링크되어 구동되는 것은 이 단계의 완료 조건이 아닙니다 — resolver가 들어 있는 소스가
아직 Linux 빌드에 포함되지 않았기 때문입니다.

## 3d-13 — 호스트 키 폴링을 SDL로

설계: [결정 5](../design/20260822-503-linux-execution-engine.md)

3d-12 이후 Linux에서 컴파일되지 않는 7개 중 셋이 이것 하나에 걸려 있습니다. 측정으로
확인했습니다 — `port_io_emulator.cpp`는 `GetAsyncKeyState` 한 줄, 나머지 둘은 그것 때문에
포함한 `<windows.h>`가 **유일한** 실패 원인입니다.

1. `HostKeyAlias::virtual_key`를 `scancode`(`SDL_Scancode`)로 바꿉니다. 필드가 하는 일은
   같습니다 — load 시점에 채워지고 스캔 경로는 읽기만 합니다.
2. `SdlKeycodeToVirtualKey`와 VK 변환표는 `SDL_GetScancodeFromKey`로 대체돼 사라집니다.
   resolve 함수는 `repiu::input`으로 옮기십시오. Win32 개념이 빠지면 그것은 더 이상 플랫폼
   코드가 아니고, `src/platform/win32/`에 남겨두면 **이름이 거짓말이 됩니다.**
3. 스캔 경로는 `SDL_GetKeyboardState()`의 포인터를 **한 번만** 받아 두고 인덱싱하십시오.
   Task 403의 제약("스캔 경로는 변환하지 않는다")이 그대로 지켜져야 하고, 키 질의 카운터는
   유지해 그 측정이 계속 같은 것을 세게 하십시오.
4. `ReadWin32ModifierState`는 `SDL_GetModState()`가 됩니다. 여섯 번 호출이 한 번이 되므로
   "any_binding_uses_modifiers일 때만 부른다"는 조건은 그대로 두되 주석의 비용 근거를
   고치십시오.
5. **레이아웃 해석 시점이 옮겨가는 것을 되돌리십시오.** `SDL_EVENT_KEYMAP_CHANGED`에서 다시
   resolve하지 않으면, 사용자가 레이아웃을 바꿨을 때 문자 키 바인딩이 조용히 어긋납니다.
6. probe의 "모든 키 이름에 Win32 가상 키가 있다" 단정을 스캔코드로 옮기십시오. 이 단정이
   있는 이유는 그대로입니다 — 이름표에 키를 더하고 폴링 경로에서 죽는 것을 막는 것입니다.
7. Windows 회귀를 확인하고 Linux 측정을 다시 돌립니다.

### 완료 조건

Windows 빌드와 probe 결과가 이전과 같아야 하고, Linux에서 컴파일되는 소스가 **76 / 80**이
되어야 합니다. `GetAsyncKeyState`가 저장소에서 사라져야 하며, 남는 것은 왜 남는지 분류합니다.

## 3d-14 — 트램폴린이 끌어오는 헤더 벽과 마지막 진단 출력

3d-13 이후 남은 넷을 측정했습니다. 빈 `windows.h` 스텁을 앞에 두고 컴파일해 진짜 의존을
드러냈습니다.

| 소스 | 오류 | 어디에 |
|---|---|---|
| `execution_trampoline.cpp` | 143 | 자기 본체 85, `win32_thread_api.h` 46, `live_telemetry_snapshot.h` 10, `exception_rescue_win32.h` 2 |
| `native_phase_sampler.cpp` | 9 | 전부 stderr 출력 한 함수 |
| `live_telemetry_snapshot.cpp` | 1 | `<psapi.h>`에서 멈춤 |
| `exception_rescue_win32.cpp` | — | 헤더가 유일한 원인 |

3d-2와 같은 모양입니다. 헤더가 58개를 만들고 있으므로 그것부터 걷어냅니다. **본체 85개는 이
단계가 아닙니다.**

1. `exception_rescue_win32.h`의 `<windows.h>`를 걷어냅니다. 남는 `LONG WINAPI ...
   (EXCEPTION_POINTERS*)` 두 선언은 3d-5가 "남은 유일한 Win32 모양"이라고 기록한 그것이므로,
   지우지 말고 **`#if defined(_WIN32)`로 울타리를 치십시오.** Linux의 대응물은 3c의
   `InstallFaultHandler`이지 이 함수가 아닙니다.
2. `live_telemetry_snapshot.h`의 `CONTEXT`는 3a의 `GuestCpuContext`로 바꿉니다. Windows에서
   같은 타입이므로 정의도 호출부도 바뀌지 않아야 합니다. 공유 메모리 RAII
   (`HANDLE`·`UnmapViewOfFile`·`CloseHandle`)는 교차 프로세스 진단이므로 울타리를 칩니다.
3. `win32_thread_api.h`는 **구성상 kernel32 함수 테이블**입니다. 통째로 울타리를 치십시오.
   중립 대체물은 트램폴린 본체가 넘어올 때 필요한 것이고, 그 모양은 네 개 호출부에서 나와야
   합니다 — 특히 `TerminateThread`는 POSIX에 대응물이 없으므로 **울타리를 치면서 그 사실을
   기록**하고 다음 단계로 넘기십시오.
4. `native_phase_sampler.cpp`의 마지막 `windows.h` 사용을 걷어냅니다. 이 파일이 `snprintf`로
   문자열을 만들고도 `WriteFile`로 내보내는 이유를 **먼저 확인하십시오.** 이유가 있다면
   중립 계층도 같은 성질을 가져야 합니다.
5. 측정을 다시 돌려 트램폴린의 오류가 자기 본체만 남는지 확인하고 숫자를 기록합니다.

### 완료 조건

Windows 빌드와 probe 결과가 이전과 같아야 합니다. 트램폴린의 남은 오류가 **전부 자기 소스
안**이어야 하고, 컴파일되는 소스가 76 / 79가 되어야 합니다. 울타리를 친 것은 각각 **왜
Windows 전용인지**와 Linux의 대응물이 무엇인지를 함께 적습니다.

## 3d-15 — 트램폴린 본체

3d-14가 헤더 벽을 걷어내 트램폴린의 실패 84개가 전부 자기 소스 안에 남았습니다. 그런데 그
84개는 파일의 **뒷부분만** 센 것입니다. 108행부터 2094행까지 약 2,000줄이
`#if defined(_WIN32)` 하나로 묶여 있어 Linux에서는 아예 평가되지 않습니다.

먼저 그 울타리를 연 사본으로 측정했습니다. **84 → 97.** 2,000줄이 숨기고 있던 것은 열세
개뿐입니다 — psapi 기반 모듈 열거 하나, `VirtualQuery` 둘, SEH 필터 상수 하나. 3d-2부터
3d-14까지가 이미 나머지를 걷어냈기 때문입니다.

1. **울타리를 열되 잘라 씁니다.** 2,000줄을 한 덩어리로 묶은 것은 Task 233의 파일 분해
   작업이지 이식의 판단이 아닙니다. 진짜 Windows 전용인 것에만 각각 울타리를 치십시오 —
   SEH `__try`/`__except` 구역, naked asm 셋, VEH 등록, psapi 모듈 열거.
2. `VirtualQuery`·`VirtualProtect`를 3b API로 옮깁니다. **보호 비트를 직접 보지 말고**
   `readable`·`writable`·`executable`로 물으십시오. 3b가 만들어진 이유가 그것이고, 3d-9가
   다섯 중 둘을 이미 회수했습니다.
3. 예외 코드 비교를 정리합니다. 제어 흐름을 정하는 것은 `FaultKind`이고, `host_code`는
   기록용입니다. `DBG_PRINTEXCEPTION_C`처럼 Windows에만 있는 코드는 이름을 붙여 울타리 안에
   두십시오.
4. 타입 표기(`DWORD`, `DWORD_PTR`, `USHORT`)를 옮길 때 3d-3의 교훈을 적용하십시오 — **폭이
   같다고 같은 타입이 아닙니다.** 필드에서 파생시키십시오.
5. asm 세 진입점(`CallGuestEntryWithStack`, `RecoverGuestStackException`,
   `RecoverHostStackException`)은 3d-12의 방식대로 **선언은 밖에, 정의는 울타리 안**에
   둡니다. GAS 대응물은 다음 단계의 일이고, Linux에서 아직 아무도 이 파일을 링크하지
   않으므로 미정의 심볼은 비용이 아니라 표시입니다.
6. `BuildDosEnvironmentBlock`의 `GetEnvironmentStringsA`는 환경 블록 **전체를 열거**합니다.
   3d-9의 헬퍼는 한 변수를 읽는 것이라 그대로 쓸 수 없습니다. POSIX의 `environ`이 대응물이며,
   계층에 추가할지 호출부에서 분기할지는 호출부를 읽고 정하십시오.

### 완료 조건

Windows 빌드와 probe 결과가 이전과 같아야 합니다. 트램폴린의 남은 오류를 **97에서 유의미하게
줄이고**, 남는 것은 왜 남는지 분류합니다. 파일 전체가 컴파일되는 것은 asm 대응물이 없어도
가능하므로, 가능하면 그것을 목표로 하십시오.

## 3d-16 — 마지막 컴파일 단위, 환경 블록, 그리고 게스트로 들어가는 세 진입점

3d-15로 실행 엔진 79개 소스 중 78개가 Linux에서 컴파일됩니다. 이 단계는 셋을 끝냅니다 —
마지막 소스 하나, 3d-15가 남긴 미기록 격차 하나, 그리고 **게스트 코드가 Linux에서 처음
실행되기 위해 필요한 어셈블리**입니다.

측정을 먼저 했습니다. `live_telemetry_snapshot.cpp`는 `<psapi.h>` **한 줄**에서 멈추고, 그
울타리를 연 사본으로 재면 오류는 **17개**뿐이며 네 곳에 몰려 있습니다 —
`OpenSharedTelemetryMapping`, `PollThreadUntilExit`, `CaptureSuspendedThreadSnapshot`, 그리고
`WriteLiveTelemetrySnapshot` 안의 `WriteFile` 한 곳입니다.

1. **`live_telemetry_snapshot.cpp`.** 헤더는 3d-14에서 이미 교차 프로세스 진단을
   `#if defined(_WIN32)` 안에 넣었습니다. `.cpp`도 **같은 경계로** 맞추십시오. 새 경계를
   발명하는 것이 아니라 헤더가 이미 그은 선을 따라가는 것이 요점입니다.
2. **`WriteLiveTelemetrySnapshot`의 `WriteFile`은 울타리 대상이 아니라 회수 대상입니다.**
   3d-14가 만든 `WriteHostErrorStream`이 정확히 이 모양 — 버퍼 하나, 잠금 없는 쓰기 한 번 —
   을 위해 존재합니다. `DWORD` 매개변수는 3d-3의 규칙대로 폭이 아니라 의미에서
   파생시키십시오.
3. **`BuildDosEnvironmentBlock`.** 3d-15 지시 6번이 미이행이고, 그 사실이 작업 로그의 남은
   항목에도 적히지 않았습니다. 지금 이 함수는 `#if defined(_WIN32)` 안에서만 환경을 열거하므로
   Linux에서는 **빈 DOS 환경 블록**을 만듭니다. 아직 이 파일이 Linux에서 링크되지 않아
   드러나지 않을 뿐입니다.
   3d-9의 `host_environment.h`는 헤더 전용 inline이고 한 변수만 읽으므로 그대로 쓸 수
   없습니다. 블록 전체 열거는 Windows에서 `GetEnvironmentStringsA`를 쓰므로 헤더에 둘 수도
   없습니다 — 3d-2가 헤더에서 걷어낸 바로 그 `windows.h`가 돌아옵니다. 선언은 헤더에,
   구현은 **양쪽 백엔드**로 두십시오. Windows는 `_environ`이 아니라
   `GetEnvironmentStringsA`를 유지합니다. CRT 복사본과 프로세스 환경 블록은 같은 것이 아니고,
   이 이식의 규칙은 Windows 동작 불변입니다.
4. **세 어셈블리 진입점을 GAS로.** 3d-12의 방식 그대로입니다.
   * `CallGuestEntryWithStack`이 **게스트로 들어가는 스택 전환 자체**입니다. `fs:[4]`·`fs:[8]`
     쌍은 3d-12가 이미 판정했습니다 — Linux에는 대응물이 없고, 그 주장은 전환된 스택에서
     폴트를 받아 재개하는 probe로 증거를 세웠습니다. 같은 결론을 여기에 적용하되 **그 이유를
     파일에 적으십시오.**
   * `StackSwitchCallState`의 오프셋은 지금 MSVC 쪽 `static_assert` 열한 개와 어셈블리 안의
     리터럴로 **두 곳에** 있습니다. GAS 판이 세 번째 사본이 되게 하지 마십시오. `.S`는 C
     전처리기를 거치므로 `#define`만 담은 헤더 하나를 양쪽이 함께 읽을 수 있습니다. 지금
     `static_assert`가 덮지 않는 네 오프셋(44·48·52·56)도 어셈블리가 이미 쓰고 있으므로 함께
     고정하십시오.
   * FPU는 저장하지 않습니다. 다섯 thunk와 달리 이 진입점이 부르는 것은 호스트 resolver가
     아니라 **게스트**이고, i386 SysV ABI는 호출 경계에서 x87 스택이 비어 있을 것을
     요구합니다. 다섯 thunk에 `fxsave`가 필요했던 3d-12의 이유가 여기에는 해당하지
     않습니다 — 이 판단도 파일에 남기십시오.
   * `RecoverGuestStackException`의 세그먼트 복원은 Linux에서 **장식이 아닙니다.** 3a의
     `StoreGuestCpuContext`는 세그먼트 레지스터를 되쓰지 않는다고 명시했으므로, 폴트 복구에서
     호스트 `%ds`·`%es`·`%fs`·`%gs`를 되돌리는 것은 오직 이 코드입니다.
5. **probe로 증거를 세우십시오.** 3d-12가 다섯 thunk에 한 것과 같습니다. `repiu_core_probe`에
   probe를 더해 **실제 출하 심볼**을 호출하십시오 — 닮은 사본이 아니라 그 심볼입니다.
   최소한 둘입니다. 합성 게스트 진입점으로 스택 전환이 실제로 일어나고 온전히 돌아오는 것,
   그리고 게스트 스택 위에서 폴트를 내고 `RecoverGuestStackException`으로 복귀해 반환값 2를
   받는 것. 두 번째가 이 단계 전체가 존재하는 이유입니다.

### 완료 조건

Linux i386 컴파일 측정이 **79 / 79**여야 합니다. Linux `repiu_core_probe`가 새 probe를 포함해
전부 통과하고, Windows Debug 빌드와 `repiu_core_probe`·`repiu_aot_probe` 결과가 이전과 같아야
합니다. 이 단계가 끝나면 남는 것은 컴파일도 어셈블리도 아니고 **링크**입니다.

## 3d-17 — 링크

3d-16이 "남은 것은 컴파일도 어셈블리도 아니고 링크"라고 적었습니다. 그 링크가 무엇을 요구하는지
**먼저 쟀습니다.** 엔진 소스 80개를 전부 오브젝트로 컴파일해 Linux 라이브러리와 함께 링크해
보면 미정의 심볼이 **아홉 개**이고, 그중 엔진 수준은 **하나도 없습니다.**

| 미정의 | 무엇 | 출처 |
|---|---|---|
| `glBindTexture`·`glDeleteTextures`·`glTexParameteri` | Glide 백엔드 | `-lGL`을 안 걸었을 뿐 |
| `mz_deflate*`·`mz_compress*` 아홉 | `rom_zip_archive.cpp` | miniz 헤더와 라이브러리의 불일치 |

**엔진 자체는 이미 링크됩니다.** 3d-2부터 3d-16까지가 Win32 API를 전부 계층으로 옮기거나
울타리 안에 넣었고, 울타리 밖에서 울타리 안의 함수를 부르는 곳이 하나도 남지 않았다는 뜻입니다.

로더 진입점도 같은 방법으로 쟀습니다. `src/host/win32/main.cpp`는 5,577줄인데 Linux에서
오류가 **열아홉 개**이고 두 군데에 몰려 있습니다 — `INFINITE` 다섯, `_putenv_s` 하나, 나머지는
`CreateProcessA` 재실행 하나. 여기서도 처음에는 spdlog 헤더 하나가 fatal error로 앞을 막고
있었습니다. 가이드가 경고하는 바로 그 함정입니다.

1. **엔진 소스를 Linux 빌드에 넣습니다.** `if(WIN32)`의 `target_sources(repiu_exe ...)` 목록을
   조건 없이 만드십시오. Linux 대응물이 있는 다섯 파일(`fault_handler_win32`,
   `virtual_memory_win32`, `worker_signal_win32`, `safe_memory_copy_win32`,
   `host_environment_win32`)은 Linux에서 빈 오브젝트가 되지만, **목록을 둘로 쪼개지
   마십시오** — 새 파일이 어느 쪽에 가야 하는지 매번 묻게 되고, 그 질문의 답은 이미
   파일 안의 울타리에 있습니다.
2. **`-lGL`.** `repiu_launcher`가 이미 같은 이유로 `GL`을 이름으로 링크합니다 — imported
   target이 아니라 이름이어야 `-m32`가 놓은 i386 라이브러리 경로에서 풀립니다.
3. **miniz 정의.** `repiu_exe`는 miniz의 include 디렉터리만 가져가고 `MINIZ_NO_*` 정의는
   가져가지 않습니다. 그래서 헤더가 라이브러리에 없는 deflate API를 노출하고, GCC가 쓰이지도
   않는 inline 래퍼를 실체화해 미정의 심볼이 됩니다. **옵션 이름을 베끼지 말고** `miniz`
   타깃의 `INTERFACE_COMPILE_DEFINITIONS`를 읽어 그대로 넘기십시오. 베낀 목록은 옵션이 바뀌는
   순간 낡습니다.
4. **`INFINITE` 다섯 곳.** 하나는 `WaitForSingleObject` 인자지만 나머지 넷은 **엔진 API를
   건너가는 값**입니다. 규약을 바꾸지 말고 이름만 중립으로 옮기십시오 —
   `repiu/runtime/execution_timeout.h`에 상수를 두고, Windows 울타리 안에서 `static_assert`로
   `INFINITE`와 같은 값임을 고정합니다. 숫자가 같으므로 동작은 한 비트도 바뀌지 않고, 같다는
   사실이 주석이 아니라 컴파일러의 단정이 됩니다.
5. **`_putenv_s`.** 3d-16의 읽기 쪽 짝이지만 **Windows 백엔드에 `SetEnvironmentVariableA`를
   쓰면 안 됩니다.** 3d-9의 `ReadEnvironmentSetting`은 `std::getenv`로 CRT 사본을 읽고,
   3d-16의 `ForEachEnvironmentEntry`는 `GetEnvironmentStringsA`로 프로세스 블록을 읽습니다.
   런처가 심은 값을 **둘 다** 보게 하는 것은 `_putenv_s`뿐입니다. 이 비대칭을 설계에
   적으십시오 — 지금은 우연히 맞아떨어지고 있고, 우연은 기록해 두지 않으면 다음 사람이
   깨뜨립니다.
6. **자식 프로세스 재실행.** Task 500이 이것을 만든 이유는 GPU 드라이버가 게스트 주소 공간을
   선점하기 때문이고, Task 502는 "Linux에도 같은 제약이 있는지는 실행 엔진이 생긴 뒤에
   판단한다"로 미뤘습니다. 이 단계의 목적은 링크이므로 **동작은 옮기되 근거는 미측정으로
   기록**하십시오. Windows는 `CreateProcessA`와 기존 명령줄 빌더를 유지하고, Linux는 argv를
   그대로 넘기는 `posix_spawn`입니다. 명령줄 빌더의 결과를 POSIX에서 다시 쪼개지 마십시오 —
   따옴표 규칙을 두 번 구현하는 일이 됩니다.
7. **Linux `repiu` 타깃.** 진입점은 Windows와 같은 소스입니다. `src/host/win32/main.cpp`를
   Linux에서 빌드하는 것이 경로 이름과 어긋나 보이지만, 3d 전체가 `src/platform/win32`에 대해
   이미 그렇게 하고 있습니다. 5,577줄을 옮기는 위험이 이름의 어색함보다 큽니다.

### 완료 조건

Linux i386에서 `repiu` 실행 파일이 **링크**되어야 합니다. Windows Debug 빌드와
`repiu_core_probe`·`repiu_aot_probe` 결과가 이전과 같고, Linux `repiu_core_probe`가 계속 전부
통과해야 합니다. 컴파일 측정은 80 / 80을 유지합니다.

실행은 이 단계의 완료 조건이 **아닙니다.** 링크되는 것과 도는 것은 다른 일이고, 둘을 한
단계에 묶으면 어느 쪽이 실패했는지 말할 수 없게 됩니다.

## 3d-18 — 스레드 계층

3d-14가 kernel32 스레드 테이블을 울타리에 넣으면서 이렇게 적었습니다 — "여덟 멤버 중 셋이
구동에 필요하고, 그 중립 대응물의 모양은 이 표가 아니라 `execution_trampoline.cpp`의 네
호출부에서 나와야 한다. `TerminateThread`는 POSIX 대응물이 없는 하나이고, Linux가 거기서
무엇을 하는지는 다음 하위 단계가 정할 일이다." 그 다음이 여기입니다.

호출부를 먼저 읽었습니다. 표의 여덟 멤버가 세 무리로 갈립니다.

| 멤버 | 어디서 | 판단 |
|---|---|---|
| `create_thread`(2), `close_handle`(2), `get_last_error` | 번역 워커와 게스트 스레드 | **중립 계층으로** |
| `get_exit_code_thread` | 폴 루프가 "아직 도는가"를 묻는 곳 | **중립 계층으로** |
| `suspend_thread`·`get_thread_context`·`resume_thread` | 정지 스냅샷과 감시견 | 3d-19 |
| `terminate_thread` | 감시견 최후 수단 | 아래 4번 |

1. **`include/repiu/platform/host_thread.h`를 넓힙니다.** 지금 이 헤더는 스레드 번호 하나만
   답합니다. 여기에 핸들 타입과 생성·조회·대기·해제를 더하십시오. 진입점 시그니처는
   `std::uint32_t (*)(void*)`입니다 — Windows의 `DWORD WINAPI(LPVOID)`와 POSIX의
   `void* (*)(void*)` 어느 쪽도 아닌, **호출부가 실제로 쓰는 모양**입니다.
2. **`STILL_ACTIVE`를 노출하지 마십시오.** Windows의 `GetExitCodeThread`는 도는 스레드에
   259를 돌려주는데 259는 **적법한 종료 코드이기도 합니다.** 지금 두 호출부가
   `!= STILL_ACTIVE`로 판정하므로, 게스트가 259로 끝나면 영원히 도는 것으로 보입니다.
   3b가 보호 비트마스크 대신 `readable`을 돌려준 것과 같은 자리입니다 — **`running`을 따로
   답하십시오.** Windows 백엔드는 `WaitForSingleObject(thread, 0)`으로 먼저 갈라야 모호함이
   사라집니다.
   **오늘 이 값이 나오지는 않습니다** — 스레드 프로시저는 0·1·2·4·5를, 감시견은 0이나 3을
   씁니다. 잠재적 결함이지 살아 있는 결함이 아니므로, 고쳤다고 적을 때 그 구분을 지키십시오.
3. **POSIX 백엔드는 자기 기록을 둡니다.** `pthread_t`는 종료 코드를 보관하지 않고
   `pthread_join`은 기다립니다. 폴 루프가 필요한 것은 **기다리지 않는 조회**이므로, 감싸는
   기록에 원자적 완료 플래그와 종료 코드를 두고 래퍼 스레드가 채우게 하십시오. 시한부 대기는
   glibc의 `pthread_timedjoin_np`가 있습니다 — 절대 시각을 요구한다는 점만 조심하십시오.
4. **`TerminateThread`의 답은 "필요 없다"입니다.** 호출부를 읽으면 그것이 **최후 수단**이고,
   그 앞에 이미 우아한 경로가 있습니다 — 스레드를 정지시키고 `RecoverToHost`로 컨텍스트를
   복귀 진입점으로 돌린 뒤 재개하는 것입니다. 그 기제는 3d-16이 Linux에서 이미 세웠습니다.
   Linux에서 그 경로는 시그널로 같은 일을 할 수 있으므로, `TerminateThread`는 **대응물을
   만들 것이 아니라 우아한 경로가 유일한 경로가 되는** 자리입니다. 이 단계에서는 그 판단만
   설계에 적고, 실제 인터럽트 구현은 폴 루프와 함께 3d-19로 넘기십시오.
5. **probe로 계약을 시험하십시오.** `repiu_core_probe`에 더해 양쪽에서 같은 단정을
   요구하십시오 — 스레드가 실제로 돌았는지, 매개변수가 그대로 도착하는지, 도는 동안 조회가
   `running`을 답하는지, 끝난 뒤 종료 코드가 그대로 나오는지, 그리고 **259를 돌려주는
   스레드가 종료로 보이는지.** 마지막 것이 2번을 증거로 만드는 단정입니다.
6. **Windows 호출부를 옮기고 회귀가 없음을 확인하십시오.** `GetWin32ThreadApi`는 아직
   `suspend`·`resume`·`get_thread_context` 때문에 남습니다. 이 단계에서 지우는 것이 목표가
   아닙니다.

### 완료 조건

같은 probe 소스가 Windows와 Linux i386에서 통과해야 합니다. Windows Debug 빌드와
`repiu_core_probe`·`repiu_aot_probe` 결과가 이전과 같고, Linux 컴파일 측정과 `repiu` 링크가
유지되어야 합니다.

게스트가 Linux에서 도는 것은 이 단계의 완료 조건이 **아닙니다.** 스레드를 만들 수 있는 것과
그 스레드가 게스트를 실행하는 것은 다른 일입니다.

---

# Linux Execution Engine Work Order (Stage 3)

Design: [20260822-503-linux-execution-engine.md](../design/20260822-503-linux-execution-engine.md)

Stage 3 is split into sub-stages; each one appends a section here as it completes.

## 3a — Register context abstraction

1. Add `include/repiu/platform/guest_cpu_context.h`: `using GuestCpuContext = CONTEXT;` on Windows,
   a structure with the **same field names** elsewhere. Leaving the field names alone is the whole
   point of this sub-stage — touching 270 sites would forfeit the "Windows behaves identically"
   property this port rests on.
2. Keep the debug-register fields (`Dr0`-`Dr3`, `Dr6`, `Dr7`) in the structure but always zero on
   Linux, because code mentioning them must still compile on both hosts.
3. Implement the `ucontext_t` conversions in `src/platform/linux/guest_cpu_context.cpp`:
   `LoadGuestCpuContext`, `StoreGuestCpuContext`, and `ReadGuestFaultInfo` for the fault address and
   access direction. A non-i386 build returns `false` rather than quietly reading the wrong layout.
4. Add a `guest_cpu_context` probe to `repiu_core_probe`: the round trip and fault extraction on
   Linux, the presence of the field set on Windows.
5. Rebuild `repiu` and `repiu_core_probe` for Windows Debug and confirm no regression.
6. Build `repiu_core_probe` for Linux i386 and confirm every probe passes.

### Completion criteria

The Windows build and probe results match what they were, and the Linux i386 `repiu_core_probe`
passes in full including the new probe. No Linux execution path appears in this sub-stage.

## 3b — Memory API abstraction

1. Read all 112 `Virtual*` call sites and write down what each actually asks. As in 3a, where
   counting the fields exposed what the design had missed, the API's shape must come from that list
   rather than from expectation.
2. Put `MemoryProtection`, `MemoryRegion`, `MemoryReservation`, and six functions in
   `include/repiu/platform/virtual_memory.h`. Do not expose a protection bitmask — answer with
   `readable`, `writable`, and `executable`, because the code currently carries five hand-written
   classifiers for that question and they disagree.
3. Implement both `src/platform/win32/virtual_memory_win32.cpp` and
   `src/platform/linux/virtual_memory.cpp`. The Windows backend exists so that one probe can hold
   both implementations to the same promises.
4. On Linux, record protections in a shadow table, since `mprotect` does not report what it
   replaced. Track reservations separately from protection intervals so `allocation_base` survives a
   protection change that splits a region.
5. Add a `virtual_memory` probe to `repiu_core_probe`. One source must run on both hosts and demand
   the same answers; a probe that checks the two implementations separately does not do this
   sub-stage's job.
6. Build and run on both, passing in full, with no Windows regression.

### Completion criteria

The same probe source passes on Windows and on Linux i386. Where the hosts are found to diverge, fix
the API to absorb the difference rather than weakening the probe. Migrating call sites is not part
of this sub-stage.

## 3c — Signal-based fault handler

1. Put `FaultKind`, `FaultEvent`, `FaultDisposition`, and install/remove in
   `include/repiu/platform/fault_handler.h`. The callback is a function pointer plus `void*`;
   nothing inside a handler may allocate.
2. Implement it with a vectored handler on Windows and `sigaction` on Linux. Because
   `GuestCpuContext` is `CONTEXT` on Windows, the callback edits the kernel's own structure with no
   copy; Linux reads through 3a's conversions and writes back on resume.
3. Linux requires `sigaltstack`, so the handler can run when the guest stack is damaged or being
   switched, and `SA_NODEFER`, because the engine plants breakpoints and arms single steps from
   inside the handler and nesting is therefore normal.
4. Catch `SIGSEGV`, `SIGBUS`, `SIGTRAP`, `SIGILL`, and `SIGFPE`, classifying by `si_code`. Whether
   `int3` arrives as `TRAP_BRKPT` or `SI_KERNEL` varies by kernel, so treat anything that is not
   `TRAP_TRACE` as a breakpoint.
5. Add a `fault_handler` probe that raises real faults and resumes from them; nothing else verifies
   this sub-stage. Write the handler so it always makes progress — resuming without changing
   anything re-runs the faulting instruction forever, and a hung probe is far worse than a failing
   one.
6. Build and run on both, passing in full, with no Windows regression.

### Completion criteria

The same probe source passes on both hosts, covering at least a read fault, a write fault including
the reported direction, and the round trip of `int3` to arming the trap flag to a single step to
disarming to a normal return. That last one is the engine's central mechanism; if it does not stand
here, 3d has nothing to stand on.

## 3d-1 — How the Linux binaries link, and the stack bridge

All of 3d — rewriting the thunks, migrating the call sites, and a real run — is too much for one
sub-stage: what remains of `src/platform/win32` is 78 files and some 42,000 lines. This one carves
off the part that every later piece of assembly depends on.

1. Settle how the Linux binaries link, by measurement. Hand-written thunks address globals directly,
   which in a PIE produces a text relocation — and a writable text segment defeats the page
   protection this engine detects self-modifying code with. Plain `-no-pie`, meanwhile, puts an i386
   image at 0x08048000, inside the 0x01000000-0x09000000 range the guest's relocated image needs.
   Find the setting that satisfies both and record why.
2. Write the bridge as a single macro in `src/platform/linux/stack_bridge.inc.S`. The five dispatch
   thunks differ only in which resolver they call, so they should be five instantiations, not five
   copies. Use Intel syntax so it reads line for line against the MSVC originals; a transcription
   error in a thunk does not announce itself.
3. Add a `stack_bridge` probe that expands the same macro rather than a lookalike written for the
   occasion, with the same contract in MSVC assembly on Windows.
4. The probe must check the frame layout is self-consistent, that the stack switch really happened,
   that editing the saved EAX in the frame reaches the caller, that a missing context is refused, and
   that the caller's stack survives.
5. Turn the `fs:[4]`/`fs:[8]` hypothesis into evidence. The design expects those 28 sites to
   disappear on Linux, and there is only one way to find out: raise a fault while on the switched
   host stack and see whether the 3c handler still delivers and resumes, since that situation is
   exactly what the TIB writes exist for on Windows.

### Completion criteria

The same probe source passes on both hosts. If a fault on the switched stack is handled on Linux with
nothing having been told about stack bounds, the hypothesis is confirmed; if not, the hypothesis was
wrong and the design has to change. No Windows regression.

## 3d-2 — Getting `windows.h` out of the headers

3d-1's measurement said 24 of the 32 sources that do not compile are stopped by a single header. This
is the first half of moving call sites onto the APIs from 3a, 3b, and 3c.

1. Remove `<windows.h>` from `execution/thread_context.h`. All it uses is `HANDLE`, three times, and
   `HANDLE` is `void*` on Windows — the same type.
2. Find every header that was relying on that include transitively, as a list. Fixing one reveals the
   next, so do not chase them one build at a time.
3. `CONTEXT` becomes `repiu::platform::GuestCpuContext` and `EXCEPTION_POINTERS` a forward
   declaration of its tag. Both resolve to the same type on Windows, so no definition and no call
   site should change.
4. Use bulk replacement, but read the result. This repository already contains forward declarations
   like `using CONTEXT = _CONTEXT;`, which a rename-the-word regex turns into a syntax error, and
   comments should be left alone.
5. Check for Windows regressions. Anything that was arriving by accident through that header — such
   as `NOMINMAX` — will surface, and it should have been declared explicitly all along.
6. Re-run the Linux measurement and record the new number.

### Completion criteria

The Windows build and probe results match what they were, and more than 46 sources compile on Linux.
Whatever still fails is classified by *why* and handed to 3d-3.

## 3d-3 — Getting the remaining Win32 API out of the call sites

3d-2 brought down the header wall; the 26 that remain all fail on real Win32 API in their own source.
Take the largest group first.

1. Before replacing the 152 `Interlocked*` calls, find out what they operate on. `std::atomic` looks
   like the answer, but if the target is a fixed layout another process maps, wrapping it changes
   what that process reads. Naming the operations and leaving the field type alone may be right.
2. Carry the return semantics across exactly: `InterlockedIncrement` returns the value after,
   `InterlockedExchange` the value before, and there are call sites that use them.
3. Move the guest store path in `guest_memory_access.cpp` onto the 3b API, keeping the
   unprotect / write / restore-the-previous-protection shape intact.
4. For `GetTickCount`, check first that every call site only takes differences; one absolute read
   would make it unreplaceable. Keep the 32-bit width, because the call sites rely on unsigned
   subtraction working across the wrap.
5. When moving type spellings, remember that the same width is not the same type: `DWORD` is
   `unsigned long` on Windows and `std::uint32_t` is `unsigned int`, so their pointers do not
   convert. Derive the type from the field instead.
6. Confirm no Windows regression and re-run the Linux measurement.

### Completion criteria

The Windows build and probe match what they were, more than 52 sources compile on Linux, and
whatever still fails is classified by why.

## 3d-4 — The exception dispatcher onto `FaultEvent`

This is the centre of what remains: roughly 300 reads of the exception structure, most of them inside
`execution_trampoline.cpp` at 5,072 lines. Work from the leaves inward rather than attempting it at
once.

1. Count, per function, what each handler actually reads from the exception structure. If it is all
   in `FaultEvent` the sub-stage is a signature change; anything missing is what 3c needs to gain.
2. `ExceptionAddress` has no Linux counterpart — `si_addr` is the data address for SIGSEGV. Measure
   whether it equals `Eip`. If it does, every use of it becomes `Eip`; if not, each needs designing.
   Take the Windows value from the exception record so the comparison is a measurement rather than a
   tautology.
3. Move one small handler first to confirm the pattern, then do the rest as a batch. The registers
   come from the event, so the separate context argument goes.
4. While the dispatcher still receives the Win32 structure, build the `FaultEvent` in one place.
   Building it per call site multiplies copies of the classification; put the builder in the 3c
   backend so the classification lives once, and mark it explicitly as transitional.
5. When removing a local, check its uses to the end of the function. In a 400-line function the eye
   misses them — leave it to the compiler, but do run the build.
6. The DBT paths synthesise an `EXCEPTION_RECORD` purely to give the dispatcher something it accepts.
   With `FaultEvent` there is nothing to synthesise; clean those up in the same pass.

### Completion criteria

The Windows build and probe match what they were and more than 56 sources compile on Linux. Changing
the dispatcher's own signature need not be part of this sub-stage; once the leaves have moved, that
becomes the natural next step.

## 3d-12 — Instantiating the five thunks from the GAS macro

The orders and outcomes for 3d-5 through 3d-11 continue in the work log. This section covers what
3d-11 named as "the last assembly work before linking and running": 3d-11 got the five
`aot_dbt_*_dispatch.cpp` compiling on Linux, but the thunk bodies are still inside
`#if defined(_MSC_VER)`, so their addresses are `nullptr` there.

1. Read the five originals against 3d-1's macro line by line. 3d-1 recorded that they differ only in
   which resolver they call, but that was the success path alone. Count them again including what
   each does when there is no context, and whatever differs has to become a parameter of the macro.
2. The refusal cannot simply be a `ret`. A dispatch site pushes its own address and then calls, so
   returning without fixing up where it returns to lands in the metadata the site pushed.
3. Instantiate all five in `src/platform/linux/aot_dbt_dispatch_thunks.S`, leaving each `.cpp` with a
   declaration on Linux and a `Get*ThunkAddress()` that returns the real symbol.
4. Add the refusal path to the `stack_bridge` probe. Today it covers the success path and "with no
   context, do nothing", which is not what four of the thunks do. The probe needs a caller shaped
   like a site, because pushing the address and calling is part of the contract.
5. Match the calling convention in the probe too. 3d-11 settled the five resolvers as stdcall, so a
   cdecl resolver in the probe would be testing the macro against a different ABI.
6. Confirm no Windows regression, and confirm the assembly, the build, and the probe on Linux.

### Completion criteria

The same probe source passes on both hosts and shows the refusal path resuming at the address the
site pushed. The Windows build and probe results match what they were. Linking and running the five
thunks on Linux is not part of this sub-stage, because the sources holding their resolvers are not in
the Linux build yet.

## 3d-13 — Host key polling onto SDL

Design: [Decision 5](../design/20260822-503-linux-execution-engine.md)

Three of the seven sources that still do not compile after 3d-12 are held by this one thing, as
measurement confirms: `port_io_emulator.cpp` fails on the single `GetAsyncKeyState` line, and the
other two on the `<windows.h>` they include for it. Nothing else in the three fails.

1. Change `HostKeyAlias::virtual_key` to `scancode` (`SDL_Scancode`). The field's job is unchanged:
   filled in at load time, read-only on the scan path.
2. `SdlKeycodeToVirtualKey` and its virtual-key table are replaced by `SDL_GetScancodeFromKey` and
   disappear. Move the resolve into `repiu::input`: with the Win32 concept gone it is no longer
   platform code, and leaving it under `src/platform/win32/` would make the name a lie.
3. On the scan path, take the `SDL_GetKeyboardState()` pointer once and index it. Task 403's
   constraint — the scan path never converts — has to survive intact, and the key-query counter stays
   so that measurement keeps counting the same thing.
4. `ReadWin32ModifierState` becomes `SDL_GetModState()`. Six calls become one, so keep the
   "only when any_binding_uses_modifiers" condition but correct the cost argument in its comment.
5. Put back what moves: re-resolve on `SDL_EVENT_KEYMAP_CHANGED`. Without it, a user changing
   keyboard layout silently misaligns every letter binding.
6. Move the probe's "every key name has a Win32 virtual key" assertion onto scancodes. Its reason is
   unchanged — it stops a key added to the name table from being silently dead on the polling path.
7. Confirm no Windows regression and re-run the Linux measurement.

### Completion criteria

The Windows build and probe results match what they were, and **76 of 80** sources compile on Linux.
`GetAsyncKeyState` is gone from the repository, and whatever still fails is classified by why.

## 3d-14 — The header wall the trampoline pulls in, and the last diagnostic write

The four that remain after 3d-13 were measured by compiling behind an empty `windows.h` stub, which
makes the real dependencies visible instead of stopping at the include.

| Source | Errors | Where |
|---|---|---|
| `execution_trampoline.cpp` | 143 | 85 in its own body, 46 in `win32_thread_api.h`, 10 in `live_telemetry_snapshot.h`, 2 in `exception_rescue_win32.h` |
| `native_phase_sampler.cpp` | 9 | all in one function that writes to stderr |
| `live_telemetry_snapshot.cpp` | 1 | stops at `<psapi.h>` |
| `exception_rescue_win32.cpp` | — | its header is the only cause |

The shape is 3d-2's again: headers produce 58 of them, so the headers go first. **The 85 in the body
are not this sub-stage.**

1. Remove `<windows.h>` from `exception_rescue_win32.h`. The two `LONG WINAPI ...(EXCEPTION_POINTERS*)`
   declarations left behind are what 3d-5 recorded as the last Win32 shape, so fence them with
   `#if defined(_WIN32)` rather than deleting them: the Linux counterpart is 3c's
   `InstallFaultHandler`, not this function.
2. In `live_telemetry_snapshot.h`, `CONTEXT` becomes 3a's `GuestCpuContext` — the same type on
   Windows, so no definition and no call site should change. The shared-memory RAII (`HANDLE`,
   `UnmapViewOfFile`, `CloseHandle`) is cross-process diagnostics, so fence it.
3. `win32_thread_api.h` is a kernel32 function table by construction; fence the whole header. The
   neutral replacement is needed when the trampoline's body moves and its shape has to come from the
   four call sites — `TerminateThread` in particular has no POSIX counterpart, so record that fact
   while fencing and hand it to the next sub-stage.
4. Take the last `windows.h` use out of `native_phase_sampler.cpp`. Find out first why a file that
   builds its string with `snprintf` then writes it with `WriteFile`; if there is a reason, the
   neutral layer has to keep the same property.
5. Re-run the measurement, confirm the trampoline's errors are all in its own source, and record the
   number.

### Completion criteria

The Windows build and probe results match what they were. Every remaining trampoline error is inside
its own source, and 76 of 79 sources compile. Each fence records why it is Windows-only and what the
Linux counterpart is.

## 3d-15 — The trampoline's body

3d-14 took away the header wall, leaving all 84 of the trampoline's failures inside its own source.
But those 84 counted only the **back half** of the file: lines 108 to 2094, some 2,000 of them, sit
inside a single `#if defined(_WIN32)` and are never evaluated on Linux at all.

Measuring first, on a copy with that fence opened: **84 becomes 97**. Thirteen is all those 2,000
lines were hiding — one psapi module enumeration, two `VirtualQuery` sites, one SEH filter constant —
because 3d-2 through 3d-14 had already taken away everything else.

1. **Open the fence, then cut it up.** Wrapping 2,000 lines in one condition was Task 233's file
   decomposition, not a decision of this port. Fence only what is genuinely Windows: the SEH
   `__try`/`__except` regions, the three naked assembly functions, the VEH registration, and the
   psapi module enumeration.
2. Move `VirtualQuery` and `VirtualProtect` onto the 3b API, asking `readable`, `writable`,
   `executable` rather than reading protection bits. That is what 3b was built for, and 3d-9 already
   collected two of the five classifiers it predicted.
3. Sort out the exception-code comparisons. `FaultKind` decides control flow and `host_code` is for
   the record; codes that exist only on Windows, such as `DBG_PRINTEXCEPTION_C`, get a name and stay
   inside a fence.
4. Moving type spellings (`DWORD`, `DWORD_PTR`, `USHORT`), apply 3d-3's lesson: the same width is not
   the same type. Derive them from the field.
5. The three assembly entries (`CallGuestEntryWithStack`, `RecoverGuestStackException`,
   `RecoverHostStackException`) follow 3d-12: declaration outside, definition inside the fence. The
   GAS counterparts are the next sub-stage's work, and since nothing links this file on Linux yet, an
   undefined symbol is a marker rather than a cost.
6. `BuildDosEnvironmentBlock` uses `GetEnvironmentStringsA`, which enumerates the **whole**
   environment block; 3d-9's helper reads one variable and does not cover it. POSIX `environ` is the
   counterpart, and whether that belongs in the layer or at the call site is decided by reading the
   call site.

### Completion criteria

The Windows build and probe results match what they were. The trampoline's remaining errors are
**materially fewer than 97**, and whatever remains is classified by why. Compiling the whole file is
possible without the assembly counterparts, so aim for that.

## 3d-16 — The last compile unit, the environment block, and the three entries into the guest

3d-15 leaves 78 of the execution engine's 79 sources compiling on Linux. This sub-stage finishes
three things: the last source, one gap 3d-15 left unrecorded, and **the assembly guest code needs
before it can run on Linux at all**.

Measured first. `live_telemetry_snapshot.cpp` stops at a **single line**, `<psapi.h>`, and a copy
with that fence opened reports only **17 errors**, gathered in four places:
`OpenSharedTelemetryMapping`, `PollThreadUntilExit`, `CaptureSuspendedThreadSnapshot`, and one
`WriteFile` inside `WriteLiveTelemetrySnapshot`.

1. **`live_telemetry_snapshot.cpp`.** 3d-14 already put the cross-process diagnostics behind
   `#if defined(_WIN32)` in the header. Make the `.cpp` agree with **the same boundary**. The point
   is to follow the line the header already drew rather than to invent a new one.
2. **The `WriteFile` in `WriteLiveTelemetrySnapshot` is to be collected, not fenced.** 3d-14's
   `WriteHostErrorStream` exists for exactly this shape — one buffer, one write, no lock. Derive the
   `DWORD` parameters from meaning rather than width, as 3d-3 requires.
3. **`BuildDosEnvironmentBlock`.** Item 6 of the 3d-15 order was not carried out, and that fact did
   not reach the work log's remaining items either. The function enumerates the environment only
   inside `#if defined(_WIN32)`, so on Linux it builds an **empty DOS environment block**. It is
   invisible only because nothing links this file on Linux yet.
   3d-9's `host_environment.h` is header-only and reads one variable, so it does not cover this.
   Enumerating the whole block uses `GetEnvironmentStringsA` on Windows, which cannot live in a
   header — that is the very `windows.h` 3d-2 took out of the headers. Declare it in the header and
   implement it in **both backends**. Windows keeps `GetEnvironmentStringsA` rather than `_environ`:
   the CRT's copy and the process environment block are not the same thing, and this port's rule is
   that Windows behaviour does not change.
4. **The three assembly entries in GAS**, following 3d-12 exactly.
   * `CallGuestEntryWithStack` **is the stack switch into the guest**. 3d-12 already settled the
     `fs:[4]` / `fs:[8]` pair: Linux has no counterpart, and that claim was held to evidence by a
     probe that takes a fault on the switched stack and resumes. Apply the same conclusion here, and
     **write the reason into the file**.
   * `StackSwitchCallState`'s offsets live in **two** places today — eleven MSVC `static_assert`s and
     literals inside the assembly. Do not let the GAS version become a third copy. A `.S` goes
     through the C preprocessor, so one header of `#define`s can be read by both. Pin the four
     offsets the `static_assert`s do not cover (44, 48, 52, 56) as well; the assembly already uses
     them.
   * No FPU save. Unlike the five thunks, what this entry calls is the **guest**, not a host
     resolver, and the i386 System V ABI requires the x87 stack to be empty at a call boundary.
     3d-12's reason for `fxsave` in the five does not apply here — record that judgement in the file
     too.
   * `RecoverGuestStackException`'s segment restores are **not decoration** on Linux. 3a's
     `StoreGuestCpuContext` states that segment registers are deliberately not written back, so this
     code is the only thing that puts the host's `%ds`, `%es`, `%fs`, `%gs` back after a fault.
5. **Hold it to evidence with a probe**, as 3d-12 did for the five thunks. Add a probe to
   `repiu_core_probe` that calls **the shipped symbols**, not a lookalike. Two scenarios at minimum:
   a synthetic guest entry showing the switch happens and returns intact, and a fault taken on the
   guest stack that recovers through `RecoverGuestStackException` and returns 2. The second is why
   this whole sub-stage exists.

### Completion criteria

The Linux i386 compile measurement reads **79 of 79**. Linux `repiu_core_probe` passes in full
including the new probe, and the Windows Debug build with `repiu_core_probe` and `repiu_aot_probe`
match what they were. When this stands, what is left is neither compiling nor assembly but
**linking**.

## 3d-17 — Linking

3d-16 wrote that what remained was neither compiling nor assembly but linking. What linking asks for
was **measured first**: compiling all 80 engine sources to objects and linking them with the Linux
library leaves **nine** undefined symbols, and **none of them are the engine's**.

| Undefined | What | Cause |
|---|---|---|
| `glBindTexture`, `glDeleteTextures`, `glTexParameteri` | the Glide backend | `-lGL` was never on the line |
| nine `mz_deflate*` / `mz_compress*` | `rom_zip_archive.cpp` | miniz's header and its library disagree |

**The engine already links.** 3d-2 through 3d-16 moved every Win32 API into a layer or behind a
fence, and the measurement says there is not one place left where code outside a fence calls
something inside one.

The loader entry point was measured the same way. `src/host/win32/main.cpp` is 5,577 lines and has
**nineteen** errors on Linux, gathered in two places: five `INFINITE`, one `_putenv_s`, and the rest
a single `CreateProcessA` relaunch. Here too a single spdlog header stood in front as a fatal error —
exactly the trap the measurement guide warns about.

1. **Put the engine sources into the Linux build.** Make the `target_sources(repiu_exe ...)` list
   inside `if(WIN32)` unconditional. The five files with Linux counterparts —
   `fault_handler_win32`, `virtual_memory_win32`, `worker_signal_win32`, `safe_memory_copy_win32`,
   `host_environment_win32` — compile to empty objects there, but **do not split the list in two**:
   it would make every new file a question about which list it belongs in, and the answer is already
   in the fence inside the file.
2. **`-lGL`.** `repiu_launcher` already links `GL` for the same reason, by name rather than as an
   imported target so it resolves in the i386 library path `-m32` puts on the search line.
3. **The miniz definitions.** `repiu_exe` takes miniz's include directory without its `MINIZ_NO_*`
   definitions, so the header exposes deflate APIs the library does not contain and GCC materialises
   the unused inline wrappers into undefined references. **Do not copy the option names**: read the
   `miniz` target's `INTERFACE_COMPILE_DEFINITIONS` and pass them through. A copied list goes stale
   the moment an option changes.
4. **The five `INFINITE` sites.** One is an argument to `WaitForSingleObject`; the other four are a
   value that **crosses the engine's API**. Do not change the convention, only the spelling: put the
   constant in `repiu/runtime/execution_timeout.h` and pin it to `INFINITE` with a `static_assert`
   inside the Windows fence. The number is the same, so no behaviour changes by a single bit, and
   that they are the same becomes an assertion the compiler checks rather than a comment.
5. **`_putenv_s`.** It is the write half of 3d-16, but the Windows backend **must not be
   `SetEnvironmentVariableA`**. 3d-9's `ReadEnvironmentSetting` reads the CRT's copy through
   `std::getenv` while 3d-16's `ForEachEnvironmentEntry` reads the process block through
   `GetEnvironmentStringsA`, and only `_putenv_s` puts a launcher-published value where **both** can
   see it. Write that asymmetry into the design: it happens to line up today, and an accident nobody
   recorded is one the next person breaks.
6. **The child-process relaunch.** Task 500 created it because a GPU driver claims the address space
   the guest needs, and Task 502 deferred the question of whether Linux has the same constraint until
   an execution engine existed. This sub-stage is about linking, so **carry the behaviour across and
   record the rationale as unmeasured.** Windows keeps `CreateProcessA` and the existing command-line
   builder; Linux passes argv straight to `posix_spawn`. Do not re-split the built command line on
   POSIX — that implements the quoting rules a second time.
7. **A Linux `repiu` target.** The entry point is the same source as on Windows. Building
   `src/host/win32/main.cpp` on Linux looks wrong against its path, but all of 3d already does that
   for `src/platform/win32`, and moving 5,577 lines is a larger risk than an awkward name.

### Completion criteria

The `repiu` executable **links** on Linux i386. The Windows Debug build with `repiu_core_probe` and
`repiu_aot_probe` match what they were, and Linux `repiu_core_probe` still passes in full. The
compile measurement stays at 80 of 80.

Running is **not** a completion criterion here. Linking and running are different things, and tying
them to one sub-stage makes it impossible to say which of the two failed.

## 3d-18 — The thread layer

When 3d-14 fenced the kernel32 thread table it wrote: three of its eight members are what running
needs, their neutral counterpart's shape has to come from the four call sites in
`execution_trampoline.cpp` rather than from the table, and `TerminateThread` is the one with no POSIX
counterpart, whose answer is for the next sub-stage to decide. This is that sub-stage.

The call sites were read first. The eight members fall into three groups.

| Member | Where | Verdict |
|---|---|---|
| `create_thread` (2), `close_handle` (2), `get_last_error` | the translation worker and the guest thread | **into the layer** |
| `get_exit_code_thread` | where the poll loop asks "is it still running" | **into the layer** |
| `suspend_thread`, `get_thread_context`, `resume_thread` | the suspended snapshot and the watchdog | 3d-19 |
| `terminate_thread` | the watchdog's last resort | item 4 below |

1. **Widen `include/repiu/platform/host_thread.h`.** Today it answers one thing, the thread's number.
   Add a handle type and create, query, wait and release. The entry signature is
   `std::uint32_t (*)(void*)` — neither Windows' `DWORD WINAPI(LPVOID)` nor POSIX's
   `void* (*)(void*)`, but **the shape the call sites actually use**.
2. **Do not expose `STILL_ACTIVE`.** Windows' `GetExitCodeThread` reports 259 for a running thread,
   and 259 is **also a legal exit code**. Both call sites decide with `!= STILL_ACTIVE` today, so a
   guest that exited with 259 would look like one that never stops. This is where 3b stood when it
   returned `readable` instead of a protection bitmask: **answer `running` separately.** The Windows
   backend has to split on `WaitForSingleObject(thread, 0)` first for the ambiguity to go away.
   **The value does not occur today** — the thread procedure returns 0, 1, 2, 4 or 5 and the watchdog
   passes 0 or 3. It is a latent defect, not a live one, and the write-up must keep that distinction.
3. **The POSIX backend keeps its own record.** `pthread_t` does not hold an exit code and
   `pthread_join` waits, while what the poll loop needs is a query that does **not** wait. Put an
   atomic completion flag and the exit code in a wrapping record and have the wrapper thread fill it.
   For the bounded wait, glibc has `pthread_timedjoin_np` — mind that it wants an absolute time.
4. **The answer for `TerminateThread` is that it is not needed.** Reading the call site shows it is
   the **last resort**, and that a graceful path already runs ahead of it: suspend the thread, point
   its context at the recovery entry with `RecoverToHost`, resume. 3d-16 already stood that mechanism
   up on Linux. There a signal can do the same work, so `TerminateThread` is not something to find a
   counterpart for — it is where **the graceful path becomes the only path.** Record that judgement
   in the design here and leave the interrupt itself to 3d-19, with the poll loop it belongs to.
5. **Hold the contract to a probe.** Add one to `repiu_core_probe` demanding the same assertions on
   both hosts: that the thread ran, that the parameter arrived unchanged, that a query answers
   `running` while it runs, that the exit code survives, and **that a thread returning 259 reads as
   exited.** The last one is what turns item 2 into evidence.
6. **Move the Windows call sites and check for regressions.** `GetWin32ThreadApi` stays, because
   `suspend`, `resume` and `get_thread_context` still use it. Deleting it is not the goal here.

### Completion criteria

The same probe source passes on Windows and on Linux i386. The Windows Debug build with
`repiu_core_probe` and `repiu_aot_probe` match what they were, and the Linux compile measurement and
the `repiu` link both hold.

The guest running on Linux is **not** a completion criterion. Being able to create a thread and that
thread executing the guest are different things.
