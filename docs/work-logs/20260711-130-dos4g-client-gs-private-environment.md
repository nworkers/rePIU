# DOS/4G client GS/private environment 작업 로그

## 결과

* 전체 BW code image에서 GS load/restore를 분류했다.
* client context restore가 `GS=ES:[DI+0Ah]`로 GS를 설치함을 확인했다.
* LINEXE data global `0090:1AB8`(file `0x202CC`)의 값 `0x20`을 private client-data selector로 확정했다.
* root population routine file `0x1E562`, store file `0x1E588`을 찾았다.
* root `0020:0042 -> 0090:059A`를 복원했다.
* `0090:059A`의 `LINEXE_LOADER` record와 `0090:0522`의 export 15개를 전부 복원했다.
* PIU가 찾는 네 export의 원본 target을 확정했다.

| Export | Target |
| --- | --- |
| `GETLOADTABLE` | `0080:26B9` |
| `GETLOADNAME` | `0080:271F` |
| `LINEXE_LOADMODULE` | `0080:1B28` |
| `LINEXE_FREEMODULE` | `0080:1B43` |

## 검증

* private-environment report 재생성 결과가 동일했다.
* `LINEXE_LOADER` name byte를 변경한 in-memory 입력이 `private module name mismatch`로 거부됐다.
* export count, name selector와 table selector, 필수 네 export 존재를 분석기가 검증한다.
* `git diff --check`를 통과했다.
* Win32 x86 Debug 전체 대상이 빌드됐다.

## 의사결정 지점

원본 export는 selector `0x80`의 16-bit protected-mode code다. 현재 Win32 host는 original 32-bit PIU code를 실행하므로 다음 중 하나가 필요하다.

1. 16-bit LINEXE execution bridge를 추가한다.
2. 네 export calling convention을 복원하고 guest-callable HLE trap/call-gate를 만든다.

프로젝트의 HLE 원칙과 범위를 고려하면 2번을 권장한다. 구조와 반환 register만 먼저 활성화하면 PIU의 후속 indirect call이 실행 불가능한 16-bit target으로 이동하므로 안전하지 않다.

# DOS/4G Client GS/Private-Environment Work Log

Recovered client GS selector `0x20`, root `0020:0042 -> 0090:059A`, the `LINEXE_LOADER` record, all 15 exports, and the four targets required by PIU. The report is reproducible, module-name drift is rejected, structural checks pass, and all Win32 x86 Debug targets build. The next decision is a 16-bit execution bridge versus calling-convention recovery plus guest-callable HLE gates; the latter matches the project architecture.
