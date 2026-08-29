# Task 521 작업 로그 — 엔진 이름에서 `Win32`를 걷어냈습니다

선행: [20260829-520](20260829-520-engine-out-of-platform-win32.md)

## 배경

Task 520이 엔진을 `src/engine/`으로 옮기면서 **파일·심볼 이름은 일부러 두었습니다** — 커밋을
검토 가능하게 유지하려고 세 번째 축으로 미룬 것입니다. 521이 그 축입니다.

## 결과

| 대상 | 규모 |
|---|---:|
| 파일 이름 | 3쌍 (6 파일) |
| 심볼 | **265개 식별자 / 2,221 출현 / 147 파일** |
| include guard | 16 파일 (`REPIU_PLATFORM_WIN32_*` → `REPIU_ENGINE_*`) |

파일 이름 셋은 **Win32 의존이 0**이었습니다.

| 전 | 후 |
|---|---|
| `aot_code_cache_win32.{cpp,h}` | `aot_code_cache.{cpp,h}` |
| `aot_page_coherence_win32.{cpp,h}` | `aot_page_coherence.{cpp,h}` |
| `breakpoint_evidence_win32.{cpp,h}` | `breakpoint_evidence.{cpp,h}` |

심볼은 접두형(`Win32Foo`)과 중위형(`DoWin32Foo`) 둘 다 처리했습니다.
`Win32AotCodeCachePlacement`(182회)가 가장 많았습니다.

## 남긴 넷 — 각각 이유가 다릅니다

| 남긴 것 | 이유 |
|---|---|
| `Win32ThreadApi`, `GetWin32ThreadApi` | **백엔드 소유** — `include/repiu/platform/win32/`가 선언 |
| `exception_rescue_win32.{cpp,h}` | **실제로 SEH를 씀** — 이름이 사실과 맞음 |
| `ReResolveWin32AotSegmentOverrides` | 접두를 떼면 **다른 함수** `ReResolveAotSegmentOverrides`(`aot_runtime_dispatch.cpp`)와 충돌 |
| `kWin32GlideProducerVertexDwordCount` | 떼면 `hle::kGlideProducerVertexDwordCount`와 같은 이름 |

**충돌은 세어서 확인했습니다** — 267개 후보 중 3건이 걸렸고, 하나씩 들여다보니 둘은 진짜
충돌이고 하나는 아니었습니다.

## 덤으로 고친 둘

**하나. 죽은 전방 선언.** `breakpoint_evidence_win32.h`가 `struct _EXCEPTION_POINTERS;`를
선언하고 있었는데, 그 API는 플랫폼 중립 3c 타입(`FaultEvent`, `GuestCpuContext`)만 씁니다.
Task 503d-2가 `<windows.h>`를 걷어낸 뒤 남은 잔여물이고 **아무 데서도 쓰이지 않습니다.**
이것이 이 파일 이름이 거짓말인지 판정하는 근거이기도 했습니다.

**둘. 없는 함수를 가리키던 주석.** `aot_page_coherence.cpp`의 주석이
`RequestWin32AotGuestPageRetirement`을 언급하는데 실제 함수는 이미
`RequestAotGuestPageRetirement`이었습니다. 이번 치환이 맞췄습니다.

## 로그 문자열은 건드리지 않았습니다

`"Win32 execution time profile enabled: {}"` 같은 **출력 텍스트**는 그대로입니다. 식별자
치환 패턴이 `Win32[대문자]`라서 뒤에 공백이 오는 로그 텍스트는 걸리지 않습니다(문자열 안에
`Win32<대문자>`가 있는지 먼저 세어 **0건**을 확인했습니다).

**의도적입니다.** 이 저장소의 분석 문서와 측정 절차가 그 로그 줄을 인용하고 있어, 바꾸면
기록된 로그 발췌가 전부 어긋납니다.

## 걸린 것 — 빌드 시스템이었지 이름이 아니었습니다

치환 후 첫 빌드가 `main.cpp.o`에서 **undefined reference 34건**으로 실패했습니다. 그런데
`main.cpp` 소스에는 옛 이름이 **0건**이었습니다.

처음에 "빌드 두 개가 같은 디렉터리에서 겹쳤다"고 진단했는데 **부정확했습니다.** 겹친 것도
사실이지만 원인은 그게 아니라 **타임스탬프**입니다 — 소스를 Windows 쪽에서 쓰고 WSL의 `make`가
DrvFs 위에서 mtime을 비교하는 구조라, 갱신된 소스를 낡은 오브젝트보다 오래된 것으로 보고
재컴파일을 건너뛰었습니다.

```bash
# WSL 쪽 시계로 mtime을 새로 찍어야 한다
find src include -name '*.cpp' -o -name '*.h' | xargs touch
```

**대량 치환 뒤 링크에서만 터지면 이름을 의심하기 전에 오브젝트가 새것인지 보십시오.**

## 검증

| 항목 | 결과 |
|---|---|
| Linux Release 빌드 | 성공, 오류 0 |
| Linux DOS/4GW 샘플 `legacy`/`dynamic` | exit 2, focus offset 0x10 — 3d-19 기준선 |
| Windows Debug 빌드 | 성공 |
| Windows Release 빌드 | 성공 |
| Windows Debug probe | **15 / 15**, 실패 0 |
| wasm | **미검증** — 이 환경에 Emscripten 없음 |

동작은 바뀌지 않았습니다. 이름만 바뀐 변경이므로 산출물이 같아야 하고, 위 넷이 그것을
확인합니다.

## 남은 것

`src/host/win32/main.cpp`의 이름 불일치는 그대로입니다 — 로더 5,577줄이고 별도 작업입니다.
