# Task 447 작업 지시 — OpenWatcom을 롤링 태그에서 불변 스냅샷으로

## 1. 문제

릴리스 CI가 **Install OpenWatcom** 스텝에서 실패했습니다.

```
Downloading open-watcom-2_0-c-win-x64.exe
OpenWatcom installer hash mismatch: 4c1830bb18ef46f8e725220aeb1abc16a179db829e6e5617604af06db975306f
At D:\a\rePIU\rePIU\scripts\install_openwatcom.ps1:23 char:5
```

`scripts/install_openwatcom.ps1`이 업스트림의 **`Current-build`** 태그에서 받습니다.
그것은 **같은 URL의 내용물이 계속 교체되는 롤링 태그**입니다. 핀은 2026-07-09에
박힌 뒤 한 번도 갱신되지 않았고, 업스트림은 최소 2026-08-03에 자산을 다시 올렸습니다.

| | 크기 | SHA256 |
|---|---:|---|
| 우리 핀(2026-07-09) | 127,884,722 | `1433db02…` |
| 업스트림 `Current-build`(08-03 재빌드) | 127,867,388 | — |
| **CI가 실제로 받은 것** | — | **`4c1830bb…`** |
| `2026-08-01-Build`(불변 스냅샷) | 127,866,777 | `089b9693…` |

CI가 받은 해시는 우리 핀도, 08-03 재빌드도 아닙니다. **그 URL 뒤의 파일이 또 바뀐
것**이고, 이것이 롤링 태그를 쓰면 안 되는 이유 그 자체입니다.

**왜 지금까지는 통과했는가.** 다운로드 캐시 키가
`openwatcom-${hashFiles('scripts/install_openwatcom.ps1')}`이라, 스크립트를 건드리지
않는 한 키가 안 바뀌고 옛 파일이 복원됩니다. **캐시가 살아 있는 동안만 통과하는
구조**였고, 캐시가 만료되자 드러났습니다. 코드 변경과는 무관합니다.

## 2. 변경

1. **URL을 불변 스냅샷 태그로.** `$ReleaseTag` 파라미터를 두고 기본값
   `2026-08-01-Build`. 핀은 `089b9693…`(직접 받아 계산함).
2. **캐시 파일 이름에 태그를 넣습니다.** 두 스냅샷의 자산 이름이 같아서, 한 경로를
   쓰면 옛 파일이 해시 검사에 걸려 실패합니다.
3. **설치 디렉터리에 스탬프(`.openwatcom-release`).** 없으면 다른 태그로 설치된
   툴체인이 marker만 만족시켜 "이미 설치됨"으로 통과하고, **로컬 툴체인이 CI와 조용히
   갈라집니다** — 이번에 고치는 것과 같은 종류의 stale-artifact 버그입니다. 태그가
   다르면 지우고 다시 풉니다.
4. **오류 메시지가 할 일을 말하게.** 기대/실제 해시, 파일 경로, 그리고 "지우고 다시
   받아라, 그래도 다르면 업스트림이 교체한 것"까지.

## 3. 감수하는 것

툴체인이 2026-07-09 나이틀리에서 2026-08-01 스냅샷으로 올라갑니다. 두 아카이브를
비교하면 **4,038개 항목 중 877개가 다릅니다**(`wcl386`·`wcc386`·`wpp386`·`wlink`
전부 포함). 샘플 baseline이 흔들릴 수 있습니다. 사용자 승인 받았습니다.

## 4. 검증

1. 스크립트: 신규 설치 / 다른 태그 교체 / 재실행 시 조기 종료 세 경로.
2. **로컬에서 샘플 스위트를 새 툴체인으로 빌드하고 baseline과 비교** — CI 왕복 전에
   baseline 영향의 크기를 먼저 압니다.
3. 회귀가 나오면 그 내용이 툴체인 차이로 설명되는지 확인한 뒤 재기록 여부를 판단합니다.

---

# Task 447 Work Order — pinning OpenWatcom to a dated snapshot

## The problem

The release CI failed on **Install OpenWatcom** with a hash mismatch. The script downloads
from upstream's **`Current-build`** tag, which is rolling: the same URL serves different
content over time. The pin was set on 2026-07-09 and never updated, and upstream rebuilt
the asset at least on 2026-08-03. The hash CI actually received, `4c1830bb…`, matches
neither our pin nor that rebuild — the file behind the URL had changed again, which is
precisely why a rolling tag cannot be pinned.

It passed until now because the download cache key is
`openwatcom-${hashFiles('scripts/install_openwatcom.ps1')}`: untouched script, unchanged
key, restored old file. The build only worked while the cache lived, and the eviction
exposed it. No code change caused this.

## The change

Take the asset from an immutable dated tag through a `$ReleaseTag` parameter defaulting to
`2026-08-01-Build`, pinned to `089b9693…` computed from a direct download. Put the tag in
the cached file's name, because two snapshots share one asset name upstream and a single
cache path would hand the previous file to the hash check. Stamp the install directory with
`.openwatcom-release`, because without it an install from another tag satisfies the marker
and the script reports success while leaving the old compiler in place — a local toolchain
silently diverging from CI's, the same class of stale-artifact bug being fixed here. And
make the error say what to do: expected and actual hashes, the file path, and how to tell a
damaged download from an upstream replacement.

## What this accepts

The toolchain moves from the 2026-07-09 nightly to the 2026-08-01 snapshot, and **877 of
the archive's 4,038 entries differ**, including `wcl386`, `wcc386`, `wpp386` and `wlink`.
The sample baseline may move. The user approved the upgrade.

## Verification

Exercise all three script paths — fresh install, replacing another tag, and an early exit
on rerun — then **build the sample suite locally against the new toolchain and compare with
the baseline**, so the size of the baseline impact is known before a CI round trip. If
regressions appear, confirm they are explained by the toolchain difference before deciding
whether to re-record.
