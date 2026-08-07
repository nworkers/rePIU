# Task 448 작업 지시 — 원본 자산 없이 샘플 스위트 빌드, 그리고 릴리스 노트

## 1. 문제

Task 447이 OpenWatcom 설치를 고치자 **그 뒤에 가려져 있던 실패**가 드러났습니다.

```
Run .\scripts\build_openwatcom_samples.ps1 -Configuration Release -SkipHostBuild
[ok] Git / CMake / Visual Studio C++ tools
Original executable was not found at MASTER\PIU_1ST\PIU\PIU.EXE.
At D:\a\rePIU\rePIU\scripts\setup_test_environment.ps1:129 char:9
```

`build_openwatcom_samples.ps1`이 `setup_test_environment.ps1`을 부르고, 그 스크립트가
`MASTER\PIU_1ST\PIU\PIU.EXE`를 **필수로 요구**합니다. 그 트리는 원본 게임 자산이라
저장소에 없고 앞으로도 들어가지 않습니다. **클린 체크아웃은 절대 만족시킬 수
없습니다** — CI든 신규 기여자든.

**그런데 이 요구는 이 호출자에게 애초에 맞지 않습니다.** OpenWatcom 샘플 스위트는
자기 DOS 프로그램을 컴파일해 로더로 돌립니다. `PIU.EXE`를 열지 않습니다.

## 2. 변경

1. `setup_test_environment.ps1`에 `-SkipGameAssets` 스위치. 켜면 자산 부재를 **경고로
   낮추고** 나머지 점검(툴체인, OpenWatcom 자가 치유 설치)은 그대로 수행합니다.
   `piu_1st` 호출자는 끈 채로 둡니다 — 그쪽은 자산이 정말 필요합니다.
2. `build_openwatcom_samples.ps1`이 그 스위치를 넘깁니다. 워크플로가 `-SkipSetup`을
   일부러 넘기지 않는 이유(차가운 캐시에서 자가 치유)를 유지하면서, 쓰지 않는 자산
   요구만 제거합니다.

## 3. 부수 작업 — 릴리스 노트

릴리스 워크플로는 `gh release create ... --generate-notes`로 커밋 제목만 나열합니다.
`docs/release-notes/<tag>.md`가 있으면 그것을 쓰도록 바꾸고(없으면 기존 동작),
v0.0.136 이후를 정리한 노트를 씁니다.

## 4. 검증

1. **자산이 없는 클린 루트에서 두 분기 확인** — 스위치 없이는 지금처럼 실패, 있으면
   경고 후 통과.
2. **남은 CI 스텝을 미리 감사** — 클린 체크아웃이 만족 못 할 입력이 더 있는지. 한 번에
   하나씩 드러나는 상황이라, 태그를 또 태우기 전에 훑습니다.

---

# Task 448 Work Order — building the sample suite without the game assets

## The problem

Fixing the OpenWatcom install in Task 447 exposed the failure hiding behind it:
`build_openwatcom_samples.ps1` calls `setup_test_environment.ps1`, which **requires**
`MASTER\PIU_1ST\PIU\PIU.EXE`. That tree is the original game data, is not in the
repository and never will be, so **a clean checkout cannot satisfy it** — CI's or a new
contributor's. And the requirement does not belong to this caller in the first place: the
OpenWatcom sample suite compiles its own DOS programs and runs them through the loader,
never opening `PIU.EXE`.

## The change

Add a `-SkipGameAssets` switch to `setup_test_environment.ps1` that demotes a missing asset
tree to a warning while keeping every other check, including the self-healing OpenWatcom
install, and pass it from `build_openwatcom_samples.ps1`. `piu_1st` callers leave it off,
because they genuinely need the assets. This preserves the workflow's stated reason for not
passing `-SkipSetup` while removing a demand for assets it does not use.

## Side task

The release workflow calls `gh release create ... --generate-notes`, which lists commit
subjects. Teach it to prefer `docs/release-notes/<tag>.md` when that file exists, falling
back to the generated notes, and write the notes covering everything since v0.0.136.

## Verification

Exercise both branches **in a clean root with no assets**: failing as CI does without the
switch, warning and continuing with it. Then **audit the remaining CI steps** for other
inputs a clean checkout cannot provide — the failures are surfacing one at a time, so look
ahead before spending another tag.
