# Task 523 — 자산 파일 이름의 대소문자를 믿지 않기

작업 지시: [20260829-523](../work-orders/20260829-523-sibling-asset-case.md) ·
작업 로그: [20260829-523](../work-logs/20260829-523-sibling-asset-case.md) ·
선행: [Task 522](20260829-522-guest-owned-breakpoint.md) ·
frontier: [linux-port-frontier](../analysis/linux-port-frontier.md)

## 배경

[Task 522](20260829-522-guest-owned-breakpoint.md)가 실제 Ubuntu의 무한 트랩을 문장으로
바꿔 놓았습니다.

```
[repiu-guest-out] Fatal error: unable to initialize DLL loader.
```

WSLg에서는 같은 코드가 정상 동작합니다. **같은 HLE가 두 호스트에서 다른 답을 낼 수 없다면,
다른 것은 호스트 쪽입니다.** 그 차이를 좁힌 것이 이 작업입니다.

## 확인됨 — 갈라지는 지점은 세 번째 서비스 호출

`REPIU_DOS_INT_TRACE`로 양쪽의 DOS/DPMI 호출 순서를 나란히 놓았습니다.

| # | WSLg | VMware Ubuntu |
|---|---|---|
| 1 | `int=21 ax=3000` | `int=21 ax=3000` |
| 2 | `int=21 ax=FF00` | `int=21 ax=FF00` |
| **3** | **`int=31 ax=0006`** | **`int=21 ax=ED2B`** |
| 4 | `int=21 ax=4A2B` | `int=21 ax=4A2B` |

`AX=FF00h`(DOS/4G 식별) **직후**에 갈라집니다. 한쪽은 DPMI로 가고, 다른 쪽은 DOS에 존재하지
않는 함수로 갑니다.

## 확인됨 — 사슬을 거꾸로 따라간 결과

`AX=FF00h` 성공 경로는 세 조건의 AND이고, 그중 호스트에 의존하는 항은 하나뿐입니다 —
`linexe_environment_active`. 그것을 한 단계씩 계측했습니다.

| 계측 | 결과 |
|---|---|
| `linexe_environment_active` | 계산 블록에 **진입조차 못 함** |
| `descriptors_registered` | `written=0` |
| `images_written` | `fits=0` |
| `glide_gate_fits` | `exports=0` ← **여기** |

`context.glide_exports`가 **비어 있었습니다.**

## 근본 원인 — `exists()`가 대소문자를 정확히 맞춥니다

```cpp
const std::filesystem::path glide_path =
    profile->executable_path.parent_path() / "Glide2x.ovl";
if (std::filesystem::exists(glide_path))   // ext4에서 거짓
```

디스크의 파일은 `glide2x.ovl`, 전부 소문자입니다.

| 호스트 | 파일시스템 | 결과 |
|---|---|---|
| Windows | NTFS | 대소문자 무시 → 찾음 |
| WSLg | DrvFs (`/mnt/e/...`) | 대소문자 무시 → 찾음 |
| **실제 Ubuntu** | **ext4** | **대소문자 구분 → 못 찾음** |

**그리고 못 찾았을 때 아무 말도 하지 않았습니다.** `if`가 통째로 건너뛰어지고 실행은
계속됩니다. 그래서 다음처럼 무너졌습니다.

```
파일 못 찾음 → glide_exports 비어 있음 → 게이트 계획 없음 → 이미지 쓰기 실패
  → 디스크립터 등록 실패 → linexe_environment_active = false
  → INT 21h AX=FF00h가 실패 경로 → DOS/4G DLL 로더 초기화 실패
  → 게스트 fatal → 자기 INT3에서 영원히 회전
```

**원인과 증상 사이에 여덟 단계가 있고, 그 사이 어디에서도 오류가 보고되지 않았습니다.**

## 결정 1 — 대소문자 무시 해석기

`ResolveSiblingAssetPath(directory, filename)`. 정확한 이름을 먼저 시도하고, 없으면 디렉터리를
훑어 대소문자 무시로 맞춥니다. 없으면 빈 경로.

아케이드 덤프는 원본 매체의 대소문자를 그대로 가져옵니다. 이 트리만 해도 `DOS4GW.EXE`와
`glide2x.ovl`이 나란히 있습니다. **한쪽만 우연히 맞았을 뿐입니다.** 그래서 둘 다 이 해석기를
지나가게 합니다.

## 결정 2 — 없으면 말하게 한다

침묵이 이 문제를 비싸게 만들었습니다. 오버레이가 없으면 경고를 남깁니다. 자산이 진짜로 없는
구성도 있으므로 오류가 아니라 경고입니다.

## 결정 3 — 다섯 개의 이미지 쓰기에 이름을 준다

`images_written`은 여섯 항의 `&&` 하나였습니다. 실패해도 **어느 것이** 실패했는지 알 수
없었고, 이 조사에서 그 한 줄을 쪼개는 데 왕복 세 번을 썼습니다. 항마다 이름을 줍니다.
단축 평가 의미는 그대로입니다.

## 검증

실제 Ubuntu에서 수정 전후.

| | 전 | 후 |
|---|---|---|
| `Glide2x` exports | 0 | **173** |
| 게이트 계획 | 무효 | 유효 (stride 8) |
| `linexe_environment_active` | false | **true** |
| 세 번째 서비스 호출 | `int=21 ax=ED2B` | **`int=31 ax=0006`** (WSLg와 일치) |
| 게스트 fatal 출력 | 1 | **0** |
| 게스트 INT3 트랩 | 814,138 | **0** |
| Glide 게이트 진입 | 0 | **96** |
| 종료 | segfault (139) | 타임아웃 (3) |

## 남은 것

SSH 세션에는 `DISPLAY`가 없어 `No available video device`로 더미 폴백을 타므로 프레임은 0입니다.
**창 자체의 확인은 VM 데스크톱에서 해야 합니다.**
