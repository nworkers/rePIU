# Task 447 작업 로그 — OpenWatcom을 불변 스냅샷으로 고정

작업지시: [20260808-447](../work-orders/20260808-447-pin-openwatcom-to-dated-snapshot.md) ·
실패한 CI: 릴리스 워크플로 `Install OpenWatcom` 스텝(v0.0.141 태그)

## 1. 근인 — 롤링 태그에 해시를 고정하고 있었습니다

```
OpenWatcom installer hash mismatch: 4c1830bb18ef46f8e725220aeb1abc16a179db829e6e5617604af06db975306f
At D:\a\rePIU\rePIU\scripts\install_openwatcom.ps1:23 char:5
```

`Current-build`는 **같은 URL의 내용물이 계속 교체되는** 태그입니다. 핀은 2026-07-09에
박힌 뒤 갱신된 적이 없습니다.

| | 크기 | SHA256 |
|---|---:|---|
| 우리 핀(2026-07-09) | 127,884,722 | `1433db02…` |
| 업스트림 08-03 재빌드 | 127,867,388 | — |
| **CI가 실제로 받은 것** | — | **`4c1830bb…`** |
| `2026-08-01-Build` | 127,866,777 | **`089b9693…`** |

CI가 받은 해시는 우리 핀도 08-03 재빌드도 아닙니다. **그 URL 뒤의 파일이 또 바뀐
것**이고, 이 표 자체가 롤링 태그를 핀으로 쓸 수 없다는 증거입니다.

**왜 지금까지 통과했는가.** 캐시 키가
`openwatcom-${hashFiles('scripts/install_openwatcom.ps1')}`이라 스크립트를 건드리지
않으면 키도 안 바뀌고 옛 파일이 복원됩니다. **캐시가 사는 동안만 통과하는 구조**였고
만료되자 드러났습니다. 코드 변경과 무관하며, Task 445·446과도 무관합니다.

## 2. 변경 — 네 가지

1. **불변 스냅샷 태그.** `$ReleaseTag` 파라미터, 기본 `2026-08-01-Build`, 핀
   `089b9693…`(직접 받아 계산). 업스트림은 월간 dated 태그를 따로 내고 그것은
   재빌드로 덮이지 않습니다.
2. **캐시 파일 이름에 태그.** 두 스냅샷의 자산 이름이 같아서 한 경로를 쓰면 **옛 파일이
   새 해시 검사에 걸려** 로컬이 즉시 깨집니다.
3. **설치 디렉터리 스탬프 `.openwatcom-release`.** 이것이 없으면 다른 태그로 깔린
   툴체인이 marker(`binnt\wcl386.exe`)만 만족시켜 "이미 설치됨"으로 통과하고, **로컬
   툴체인이 CI와 조용히 갈라집니다** — 이번에 고치는 것과 정확히 같은 종류의
   stale-artifact 버그입니다. 태그가 다르면 지우고 다시 풉니다.
4. **오류 메시지가 할 일을 말합니다.** 기대/실제 해시, 파일 경로, 그리고 손상된
   다운로드와 업스트림 교체를 구분하는 방법까지.

## 3. 검증

| 검사 | 결과 |
|---|---|
| 신규 설치 | 받고 풀고 스탬프 기록 |
| **다른 태그 교체** | `Replacing OpenWatcom an unstamped build with 2026-08-01-Build` |
| 재실행 | `already installed`로 조기 종료 |
| 샘플 **빌드** | **819/819 컴파일 성공**(exit 0) |
| 샘플 **baseline 비교** | **회귀 0** |

## 4. 툴체인 교체는 baseline을 흔들지 않았습니다

두 아카이브는 **4,038개 항목 중 877개가 다릅니다** — `wcl386`·`wcc386`·`wpp386`·`wlink`
전부 포함이라 baseline 이동을 각오했는데, 실측은 **완전 동일**이었습니다.

| | 값 |
|---|---:|
| baseline | v0.0.135 (2026-08-06) |
| **회귀** | **0** |
| 신규 통과 | 0 |
| 변화 없는 실패 | 291 |
| 신규/누락 샘플 | 0 / 0 |
| 통과 | 528 / 819 (RunPassRate 66.6%) |

**재기록은 필요 없습니다.** 각오한 위험이 실측으로 해소된 경우이고, CI 왕복 전에
로컬에서 잰 덕분에 태그를 한 번 더 태우지 않았습니다.

## 5. 남긴 것

* `tools/downloads/open-watcom-2_0-c-win-x64.exe`(옛 07-09 나이틀리, 128MB)는 이제
  참조되지 않습니다. 지우지 않고 뒀습니다 — 그 툴체인 아카이브의 유일한 로컬 사본이라,
  버릴지는 사용자 판단입니다.
* 스크립트가 바뀌었으므로 CI의 OpenWatcom 캐시 키도 바뀝니다. 다음 실행은 새 스냅샷을
  한 번 받고, 그 다음부터 캐시에 얹힙니다. **의도된 동작입니다** — 캐시가 핀 만료를
  가리던 것이 이번 문제의 절반이었습니다.
* **다음 태그는 `v0.0.142`여야 합니다.** version gate가 VERSION과 태그의 일치를
  요구하고, 이 커밋이 VERSION을 0.0.142로 올립니다.

---

# Task 447 Work Log — pinning OpenWatcom to a dated snapshot

## 1. Root cause

The release CI failed on `Install OpenWatcom` with `hash mismatch: 4c1830bb…`. The script
downloaded from upstream's `Current-build`, a tag whose asset is replaced in place, against
a hash pinned on 2026-07-09 and never updated. The hash CI received matches neither our pin
nor the 2026-08-03 rebuild — the file behind that URL had changed again, which is the whole
argument against pinning a rolling tag.

It passed until now because the cache key is
`openwatcom-${hashFiles('scripts/install_openwatcom.ps1')}`: an untouched script means an
unchanged key and a restored old file. The build only worked while the cache lived. No code
change caused this, and it is unrelated to Tasks 445 and 446.

## 2. The change

The asset now comes from an immutable dated tag through a `$ReleaseTag` parameter
defaulting to `2026-08-01-Build`, pinned to `089b9693…` computed from a direct download.
The cached file carries the tag in its name, because two snapshots share one asset name and
a single path would fail the new hash check against the old file. The install directory is
stamped with `.openwatcom-release`, because otherwise an install from another tag satisfies
the `binnt\wcl386.exe` marker and the script reports success while leaving the old compiler
in place — a local toolchain silently diverging from CI's, the same class of stale-artifact
bug being fixed here. And the error now states the expected and actual hashes, the file
path, and how to tell a damaged download from an upstream replacement.

## 3. Verification

All three script paths were exercised: a fresh install, replacing an unstamped build, and
an early exit on rerun. The sample suite then built **819 of 819** with the new toolchain
and compared against the baseline.

## 4. The toolchain change did not move the baseline

**877 of the archive's 4,038 entries differ** between the two snapshots, including
`wcl386`, `wcc386`, `wpp386` and `wlink`, so a baseline shift was expected. Measured
against baseline v0.0.135: **zero regressions**, zero new passes, 291 unchanged failures,
no new or missing samples, 528 of 819 passing. **No re-recording is needed** — an accepted
risk that measurement dissolved, and measuring locally saved spending another tag to learn
it.

## 5. What is left

The old `tools/downloads/open-watcom-2_0-c-win-x64.exe`, the 2026-07-09 nightly, is no
longer referenced; it was left in place because it is the only local copy of that
toolchain archive and discarding it is the user's call. Changing the script changes CI's
OpenWatcom cache key, so the next run downloads the snapshot once and caches it afterwards
— intended, since a cache concealing an expired pin was half of this problem. And the next
tag must be **`v0.0.142`**, because the version gate requires VERSION and the tag to agree
and this commit raises VERSION.
