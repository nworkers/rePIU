# 런처 업데이트와 스팀덱 설치 / Launcher updates and a Steam Deck install

설계: [20261010-i048](../design/20261010-i048-launcher-self-update.md) ·
작업 로그: [20261010-i048](../work-logs/20261010-i048-launcher-self-update.md) ·
릴리스 아티팩트: [release-and-ci](release-and-ci.md)

이 문서는 **반복 수행하는 절차**만 담습니다. 특정 실행의 증거는 작업 로그에 있습니다.

## 1. 무엇을 하는가

런처(인자 없이 실행한 `repiu`)가 열리면 GitHub의 최신 릴리스를 확인합니다. 더 새 버전이 있으면 런처 맨
위에 "rePIU v<버전> is available."과 **Update** 버튼이 나옵니다. 누르면 이 빌드에 맞는 아카이브를 받아
GitHub가 알려 준 SHA-256과 비교하고, 같을 때만 풀어 **아카이브에 든 파일만** 바꾼 뒤 런처를 다시
시작합니다. `roms/`, `cfg/`, `nvram/`은 손대지 않습니다.

| 빌드 | 받는 파일 |
|---|---|
| Win32 | `rePIU-v<버전>-win32.zip` |
| Linux x64 (스팀덱) | `rePIU-v<버전>-linux-x64.tar.gz` |
| Linux i386 | `rePIU-v<버전>-linux-i386.tar.gz` |

Update 버튼은 **릴리스 아카이브를 푼 폴더**(실행 파일 옆 `VERSION`이 실행 중인 버전과 같음)에서만
나옵니다. 빌드 트리에서는 알림 줄과 릴리스 페이지 주소만 보입니다.

## 2. 스팀덱에 처음 설치하기

1. 데스크톱 모드에서 릴리스 페이지의 `rePIU-v<버전>-linux-x64.tar.gz`를 받아 풉니다. 예:
   ```bash
   mkdir -p ~/rePIU && cd ~/rePIU
   tar -xzf ~/Downloads/rePIU-v*-linux-x64.tar.gz --strip-components=1
   ```
2. 롬은 `~/rePIU/roms/`에 둡니다(README의 롬 배치와 같음). 설정과 NVRAM은 `~/rePIU/cfg/`,
   `~/rePIU/nvram/`에 생깁니다.
3. Steam에서 "비 스팀 게임 추가"로 `~/rePIU/repiu`를 등록하고, 속성의 **시작 위치를 `~/rePIU`**로
   둡니다. 롬과 설정은 시작 위치 기준으로 찾습니다.
4. 게임 모드에서 실행하면 런처가 열립니다. 게임패드로 롬셋을 고르고 `A`로 시작합니다.
5. 끝낼 때는 **LT + RT + 왼쪽·오른쪽 스틱 누르기를 1초** 함께 누릅니다. 게임 중이면 런처로 돌아가고,
   런처에서 한 번 더 하면 rePIU가 끝나 Steam으로 돌아갑니다(issue #52).
6. 게임 중 설정(전체 화면, 비율, 셰이더 등)은 **LT + RT + Y**로 OSD를 열어 D-pad·`A`로 바꾸고 `B`로 닫습니다
   (issue #55).

이후로는 런처에 알림이 뜨면 Update를 누르기만 하면 됩니다(D-pad로 버튼까지 올라가 `A`). Linux에서는
같은 프로세스가 새 실행 파일로 바뀌므로 Steam은 게임이 계속 실행 중인 것으로 봅니다.

## 3. 끄기

| 방법 | 범위 |
|---|---|
| `cfg/repiu.ini`의 `[Launcher]`에 `check_updates = 0` | 계속 |
| `REPIU_UPDATE_CHECK=0` | 그 실행 (파일보다 우선) |

인자 실행(`repiu pumpitea`)과 probe, 측정 스크립트는 런처를 거치지 않으므로 확인하지 않습니다.

## 4. 문제가 생기면

로그(콘솔 또는 `repiu_log.txt`)의 `update:` 줄이 단계와 이유를 말합니다.

| 로그 | 뜻 / 조치 |
|---|---|
| `check failed: cannot run curl` | Linux에 `curl`이 없음. 배포판 패키지로 설치 |
| `check failed: curl exit 6/7/28` | 네트워크 없음·연결 실패·시간 초과. 다음 실행 때 다시 확인 |
| `not a release install (...)` | 빌드 트리이거나 `VERSION`이 다름. 릴리스 아카이브로 설치 |
| `the install folder cannot be written` | 폴더 권한. 쓸 수 있는 곳에 설치 |
| `does not match the release's SHA-256` | 받은 파일이 손상됨. Retry |
| `install failed: ...` | 교체 중 실패. 기존 파일로 되돌린 상태이며 Retry 가능 |

교체 직후 옛 파일은 `<이름>.repiu-old`로 남았다가 다음 시작 때 지워집니다(목록: `.repiu-old-files`).
되돌리고 싶으면 다음 시작 전에 `.repiu-old`를 원래 이름으로 바꾸면 됩니다. 받는 중 임시 파일은
설치 폴더의 `.repiu-update/`에 있고 다음 업데이트 때 지워집니다.

## 5. 업데이트 경로 시험하기

새 릴리스를 만들지 않고 실제 GitHub 릴리스로 시험합니다.

```bash
t=build/update-test; rm -rf "$t"; mkdir -p "$t"
cp build/linux_x64_release/repiu "$t/"          # 시험할 빌드
printf '0.0.1\n' > "$t/VERSION"                 # 옛 버전인 척
REPIU_UPDATE_CURRENT_VERSION=0.0.1 "$t/repiu"   # 저장소 루트에서 실행
```

런처에 최신 릴리스 알림이 뜨고 Update로 `$t`의 파일이 바뀌며, 다시 열린 런처는 그 릴리스입니다.
`REPIU_UPDATE_CURRENT_VERSION`은 다시 시작할 때 지워집니다. 네트워크 없는 확인은 core probe의
`launcher_update`입니다.

---

# Launcher updates and a Steam Deck install

Design: [20261010-i048](../design/20261010-i048-launcher-self-update.md) · work log:
[20261010-i048](../work-logs/20261010-i048-launcher-self-update.md) · release artifacts:
[release-and-ci](release-and-ci.md). This document holds **repeatable procedures** only; the evidence
of particular runs is in the work log.

## 1. What it does

When the launcher (`repiu` with no arguments) opens, it checks GitHub's latest release. When it is newer,
the top of the launcher shows "rePIU v<version> is available." with an **Update** button, which downloads
the archive for this build, checks it against the SHA-256 GitHub reports, unpacks it only on a match,
replaces **only the files in the archive**, and restarts the launcher; `roms/`, `cfg/` and `nvram/` are
untouched. Win32 takes `rePIU-v<version>-win32.zip`, Linux x64 (Steam Deck) `-linux-x64.tar.gz`, Linux
i386 `-linux-i386.tar.gz`. The button appears only in **a folder unpacked from a release archive** (a
`VERSION` next to the executable equal to the running version); a build tree gets only the notice and the
releases page address.

## 2. First install on a Steam Deck

1. In desktop mode, download `rePIU-v<version>-linux-x64.tar.gz` from the releases page and unpack it, for
   example with `mkdir -p ~/rePIU && cd ~/rePIU && tar -xzf ~/Downloads/rePIU-v*-linux-x64.tar.gz
   --strip-components=1`.
2. Put the ROMs in `~/rePIU/roms/` (as the README's ROM layout says); settings and NVRAM appear in
   `~/rePIU/cfg/` and `~/rePIU/nvram/`.
3. In Steam, "Add a Non-Steam Game" with `~/rePIU/repiu`, and set its **Start In to `~/rePIU`**: ROMs and
   settings are found relative to it.
4. Run it in game mode; the launcher opens, choose a ROM set with the gamepad and start it with `A`.
5. To quit, hold **LT + RT + both stick clicks for a second**: a game returns to the launcher, and doing it
   again there ends rePIU and returns to Steam (issue #52).
6. In a game, open the OSD with **LT + RT + Y** to change settings (fullscreen, aspect, shader and so on) with
   the D-pad and `A`, and close it with `B` (issue #55).

From then on, press Update when the launcher shows the notice (D-pad up to the button, then `A`). On Linux
the same process turns into the new executable, so Steam sees the game as still running.

## 3. Turning it off

`check_updates = 0` under `[Launcher]` in `cfg/repiu.ini` turns it off for good; `REPIU_UPDATE_CHECK=0`
for one run, over the file. Argument runs (`repiu pumpitea`), probes and measurement scripts bypass the
launcher and never check.

## 4. When something goes wrong

The log's `update:` lines (console or `repiu_log.txt`) say which step failed and why: `cannot run curl`
(Linux has no `curl`; install it from the distribution), `curl exit 6/7/28` (no network, no connection,
timeout; the next run checks again), `not a release install (...)` (a build tree or a different
`VERSION`; install from a release archive), `the install folder cannot be written` (permissions; install
somewhere writable), `does not match the release's SHA-256` (a damaged download; Retry), `install
failed: ...` (a replacement failed; the old files are back and Retry works). After an install the old files
stay as `<name>.repiu-old` until the next start removes them (listed in `.repiu-old-files`); to go back,
rename them to their names before then. Downloads in progress live in the install folder's `.repiu-update/`
and are cleared by the next update.

## 5. Testing the update path

Use a real GitHub release, with no new release needed: copy the build to test into an empty folder, write
an old version into its `VERSION`, and run it from the repository root with the same version in
`REPIU_UPDATE_CURRENT_VERSION` (commands above). The launcher announces the latest release, Update replaces
the folder's files, and the launcher that reopens is that release. `REPIU_UPDATE_CURRENT_VERSION` is
dropped on the restart. The network-free check is the core probe's `launcher_update`.
