# Task 719 설계 — 장면별 시각을 두 host에서 비교

## 목적

Task 718에서 게임이 되돌리는 틱 카운터의 주기가 Win32 77초, Linux 84초로 9% 달랐고,
첫 초기화 전 카운터도 Linux가 컸다(8,064 대 6,406). Linux가 **어느 장면에 더 오래
머무는지** 찾는다.

## 문제

`REPIU_GLIDE_PIXEL_DIAG`는 swap 번호로 표본을 뜬다(1–60, 이후 200마다, 4,000까지).
두 host의 초당 swap 수가 다르므로 같은 번호가 같은 시각이 아니고, 평균 색 하나로는
비슷한 장면을 구분하기 어렵다.

## 설계

`REPIU_GLIDE_PIXEL_DIAG_INTERVAL_MS=<ms>`를 두면 픽셀 진단이 **시간 기준**으로 바뀐다.

```text
[repiu-scene] t_ms=N swap=K non_black=P avg=R,G,B grid=XXXXXXXXXXXXXXXX
```

* `t_ms`: 첫 swap 이후 경과 ms. 간격마다 최대 한 번 표본을 뜨고 4,000 상한은 없다.
* `grid`: 화면을 4×4로 나눈 각 칸의 평균 밝기를 16진 한 자리(0–F)로 적은 16글자.
  장면 구분용 서명이다.
* 기존 swap 기준 출력은 이 변수가 없을 때 그대로다.

분석은 로그 밖에서 한다. 인접 표본의 grid 차이가 크면 장면 경계로 보고, 두 host의
장면 순서를 맞춘 뒤 장면별 지속 시간을 비교한다. Glide 백엔드는 두 host 공용이므로
같은 코드가 같은 형식을 낸다.

## 검증

* 변수가 없을 때 출력 변화 없음
* 두 host 180초 기록, 장면 경계와 지속 시간 표

---

## English

### Purpose

In Task 718 the tick counter the game winds back cycled every 77 seconds on Win32
and 84 on Linux, 9% apart, and Linux's count before the first reset was larger
(8,064 against 6,406). Find **which scene Linux lingers in**.

### Problem

`REPIU_GLIDE_PIXEL_DIAG` samples by swap number (1–60, then every 200, up to 4,000).
The hosts swap at different rates, so the same number is not the same moment, and one
average colour separates similar scenes poorly.

### Design

With `REPIU_GLIDE_PIXEL_DIAG_INTERVAL_MS=<ms>` the pixel diagnostic samples **by
time** and writes
`[repiu-scene] t_ms=N swap=K non_black=P avg=R,G,B grid=XXXXXXXXXXXXXXXX`.
`t_ms` is milliseconds since the first swap; at most one sample per interval, with no
4,000 cap. `grid` is sixteen hex digits, the mean brightness (0–F) of each cell of a
4×4 split of the screen — a signature for telling scenes apart. Without the variable
the swap-based output is unchanged.

Analysis happens outside the log: a large grid change between neighbouring samples
marks a scene boundary; the scene order is matched across hosts and per-scene
durations compared. The Glide backend is shared, so both hosts produce the same
format from the same code.

### Verification

* No change in output when unset.
* 180-second recordings on both hosts, with a table of scene boundaries and
  durations.
