# Task 523 작업 로그 — 자산 대소문자 해석

설계: [20260829-523](../design/20260829-523-sibling-asset-case.md) ·
작업 지시: [20260829-523](../work-orders/20260829-523-sibling-asset-case.md) ·
선행: [Task 522](20260829-522-guest-owned-breakpoint.md)

## 근본 원인

```cpp
profile->executable_path.parent_path() / "Glide2x.ovl"
```

디스크의 파일은 `glide2x.ovl`입니다. NTFS와 DrvFs는 대소문자를 무시하므로 Windows와 WSLg는
찾았고, **ext4는 구분하므로 실제 Ubuntu는 못 찾았습니다.** 못 찾은 사실은 어디에도 기록되지
않았습니다.

## 한 일

* `ResolveSiblingAssetPath()` 신규 — 정확한 이름 우선, 없으면 대소문자 무시 순회.
* `DOS4GW.EXE`와 `Glide2x.ovl` **둘 다** 이 해석기를 지나가게 함.
* 오버레이 부재 시 경고 로그 추가.
* `images_written` 여섯 항을 이름 있는 항 다섯으로 분해.
* 조사용 임시 계측 다섯 블록과 그에 딸린 include 제거.

## 어떻게 찾았는가

`AX=FF00h` 성공 경로에서 호스트에 의존하는 항은 `linexe_environment_active` 하나뿐이라는
점에서 출발해, 계측을 한 단계씩 앞으로 옮겼습니다.

| 계측 단계 | 결과 |
|---|---|
| `linexe_environment_active` | 계산 블록 미진입 |
| `descriptors_registered` | `written=0` |
| `images_written` | `fits=0` |
| `glide_gate_fits` | **`exports=0`** |

네 번의 왕복이 필요했던 이유는 `images_written`이 여섯 항 `&&` 하나였기 때문입니다. 그래서
그것을 쪼개는 것을 이 작업에 포함했습니다.

## 측정 — 실제 Ubuntu, `pumpit1`

| | 전 | 후 |
|---|---:|---:|
| `Glide2x` resident exports | 0 | **173** |
| 게이트 계획 유효 / stride | 무효 / 0 | **유효 / 8** |
| 이미지 쓰기 (client·gate·glide·bss·private) | 1·1·0·0·0 | **모두 1** |
| `linexe_environment_active` | false | **true** |
| 세 번째 서비스 호출 | `int=21 ax=ED2B` | **`int=31 ax=0006`** |
| 게스트 fatal 출력 | 1 | **0** |
| 게스트 INT3 트랩 | 814,138 | **0** |
| Glide 게이트 진입 | 0 | **96** |
| 종료 | segfault (139) | 타임아웃 (3) |

세 번째 호출이 WSLg와 **정확히 일치**하는 것이 이 수정의 서명입니다.

## 정정

[Task 522] 진행 중 `docs/analysis/dll-loader-int21-ff00.md`를 인용해 "`AX=FF00h` HLE가 `AL=0`을
반환하는 것이 원인"이라고 말했습니다. **틀렸습니다.** 그 핸들러는
`kDos4gwIdentificationAxResult`와 `kDos4gwClientDataSelector`를 이미 구현하고 있고, `AL=0`은
세 게이트 중 하나가 실패했을 때 타는 **폴백**입니다. 실패한 게이트는
`linexe_environment_active`였고, 그 원인이 위의 파일 이름 대소문자입니다.
해당 분석 문서를 갱신했습니다.

## 남은 것

SSH 세션에는 `DISPLAY`가 없어 SDL이 더미 폴백을 타고 `frames=0`입니다.
**창 확인은 VM 데스크톱에서 해야 합니다.**

Tasks 505–519의 "확인됨"은 전부 **WSLg 한정**이었습니다 — 이 작업이 두 환경이 갈릴 수 있음을
보였으므로, frontier의 해당 주장 범위를 좁혔습니다.

## 검증

* Linux i386 (실제 Ubuntu, 커널 7.0.0-30-generic): 빌드 통과, 위 측정.
* Windows x86 debug: 빌드 통과.
