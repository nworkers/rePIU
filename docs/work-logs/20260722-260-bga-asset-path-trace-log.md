# 작업 로그 — BGA 자산 경로 추적: RES 아카이브 포맷 확정과 자산 계층 분리 / Work Log — BGA Asset Path Trace

* 작성일 / Date: 2026-07-22 (Task 260)
* 브랜치 / Branch: `claude/bga-asset-path-trace`
* 선행 / Predecessor: Task 259 (배경 미표시를 Glide 밖으로 격리)

## 1. 목적 / Goal

Task 259가 "게임이 배경 draw를 발행하지 않는다"까지 좁혔다. 그 상류인 자산 로딩
경로를 추적해 어디까지 정상이고 어디서 끊기는지 확정한다.

## 2. 추가한 계측 / Instrumentation

`RecordDosOpen`은 이미 존재했으나 stderr로 나오지 않아 실행 중 관측이 불가능했다.
`REPIU_DOS_ASSET_TRACE` env-gated 추적을 추가했다.

* **열기:** 성공·실패 **전건**. 게스트 경로와 해석된 호스트 경로를 함께 기록.
* **읽기·시크:** 핸들이 아니라 **DOS 경로명**으로 기록. 핸들 번호만으로는 어느
  자산이 소비되는지 알 수 없다.
* 실패와 short read는 상한 없이, 성공은 상한을 두어 반복 루프에 묻히지 않게 했다.

## 3. 확정한 사실 / Findings

### 3.1 자산 계층 (확인됨)

```
SPR.RES  (105,473 B, 344 × .SPR)   텍스트 스크립트 — 배치·애니메이션 정의
    └─ 참조 ─→
PIU.DAT  (8,751,057 B, 465 × .PTX)  실제 텍스처 픽셀
```

`.SPR`이 픽셀을 담지 않고 `TYPE ANI`/`TYPE TILE`/`NUM n` 형태의 **텍스트 스크립트**
라는 것이 이번 작업의 가장 구조적인 발견이다. 게임이 이를 `strtok`으로 파싱하는
것과 정합한다.

### 3.2 `RES\0` 아카이브 포맷 (확인됨)

두 아카이브가 동일 포맷이며, 헤더(magic/version/count/data_bytes) + 24바이트 엔트리
(`name[16]`, `size`, `offset`), 데이터 base = `0x10 + count*24`. 상세는
`docs/analysis/res-ptx-resource-loading.md`.

**무결성:** 범위 초과·간극·겹침 모두 0건, 마지막 엔트리가 파일 끝과 정확히 일치.

### 3.3 `.TGA` → `.PTX` 치환 (확인됨)

사용자 제보("확장자를 PTX로 바꿔 읽는 것 같다")가 맞았다. `logo_a.tga` 열기 실패와
`PIU.DAT` 내 `LOGO_A.PTX`(@0x1228)가 대응한다. 기존 문서의 `hfont1.tga` →
`HFONT1.PTX` 관측이 개별 사례가 아니라 **일반 규칙**임을 확인했다.

### 3.4 CD 추출 완전성 (확인됨)

CHD 데이터 트랙은 6,955섹터(≈14 MB)뿐이고 나머지 40여 트랙은 전부 CD 오디오다.
추출된 120파일 14 MB가 데이터 트랙 전량이며 **누락이 없다**.

### 3.5 read ABI 수정 유지 (확인됨)

기존 문서가 기록한 32-bit read ABI 복원이 현재도 유효하다. `PIU.DAT`이
`want=8735744 got=8735744`로 한 번에 읽히고 최종 seek이 파일 크기와 일치한다.
read·seek 오류 0건. **회귀 없음.**

## 4. 오진할 뻔한 것 2건 / Two Near-Misdiagnoses

기록으로 남긴다. 둘 다 "실패처럼 보이는 정상"이었다.

1. **열기 실패 77/109건.** 처음 이 로그를 보고 "배경 자산 로딩 실패"로 단정할
   뻔했다. 아카이브 내용을 확인해 보니 실패한 이름들이 전부 `SPR.RES`/`PIU.DAT`
   안에 있었다 — 개별 파일을 먼저 시도하고 없으면 아카이브에서 읽는 **오버라이드
   관례**다.
2. **`SHORT read` 태그.** 내가 붙인 계측이 과잉이었다. 요청보다 적게 반환되는
   read는 대부분 게임의 4096바이트 루프가 만나는 **정상 EOF**다. 결함 신호로
   설계했더니 거짓 양성이 쏟아졌다.

교훈: 로그 태그를 "실패"로 이름 붙이기 전에 **정상 동작의 모양**을 먼저 확인해야
한다. 계측 자체가 오진을 유도할 수 있다.

## 5. 미확정 / Open

* **`.PTX` 픽셀 포맷.** 매직 뒤 12바이트가 파일 크기와 무관하게 거의 동일
  (`256, 32, 32, 129, 300, 0`)하므로 해상도 필드가 아니다. 데이터부에 반복 패턴이
  있어 압축 또는 팔레트 기반 가능성.
* **PTX 465개 vs 런타임 텍스처 2~3개.** 디코드→업로드 경로가 어디서 멈추는지가
  배경 미표시의 다음 관문.
* **`.SPR` strtok 파싱 재검증.** 과거 "빈 텍스처 슬롯의 strtok NULL은 정상"이라는
  결론이 있는데, `.SPR`이 텍스트 스크립트임이 확정된 지금 그 전제가 유효한지 다시
  확인해야 한다(사용자 요청, 다음 작업).

## 6. 검증 / Verification

Win32 x86 Debug 빌드 성공. `aot-dynamic pumpit1` 구동에서 추적이 정상 동작하며,
아카이브 파싱은 별도 정적 분석(Python)으로 교차 검증했다 — 두 아카이브 모두 엔트리
합이 파일 크기와 바이트 단위로 일치.

---

## English Summary

Task 259 narrowed the missing background to "the game never issues background
draws"; this task traced the asset path upstream of that. Added an env-gated DOS
asset trace (`REPIU_DOS_ASSET_TRACE`) that logs opens, reads, and seeks by DOS
path name rather than handle.

Findings: `SPR.RES` (344 `.SPR`) and `PIU.DAT` (465 `.PTX`) share one `RES\0`
archive format, and both verify byte-exact with no gaps or overlaps. `.SPR` files
are **text scripts** (`TYPE ANI`/`TYPE TILE`/`NUM n`) describing placement, not
pixels — the pixels are all `.PTX`. The user's recollection that `.TGA` is
substituted to `.PTX` is confirmed and generalizes the existing `hfont1.tga`
observation. CD extraction is complete (the data track is only ~14 MB; the rest
is CD audio), and the previously restored 32-bit read ABI still holds — `PIU.DAT`
is read to its last byte with zero errors.

Two things that look like failures are normal and are recorded as such: 77 of 109
loose-file opens fail because the game tries a loose override before the archive,
and most short reads are ordinary EOF. The second was a flaw in my own
instrumentation — naming a tag "SHORT" before checking what healthy behavior
looks like produced a flood of false positives.

Open: the `.PTX` pixel format (post-magic fields are constant across files of very
different sizes, so not dimensions), the gap between 465 PTX entries and the two
or three textures that reach Glide, and re-verifying the earlier conclusion about
`strtok` NULL on empty texture slots now that `.SPR` is known to be a text script.
