# 작업 로그: 런처의 업데이트 확인과 제자리 설치 (issue #48)

설계: `docs/design/20261010-i048-launcher-self-update.md`
작업 지시: `docs/work-orders/20261010-i048-launcher-self-update.md`
가이드: `docs/guides/launcher-update.md`

## 한 일

* **`repiu::update`**(플랫폼 공용): `semantic_version`(`v` 유무, 숫자 비교, `BuildVersionText`),
  `release_info`(깊이 제한이 있는 작은 JSON 파서, `tag_name`·`html_url`·`assets[]`의 이름·크기·URL·
  `sha256:` 다이제스트, 빌드별 asset 이름), `sha256`(FIPS 180-4, 파일 단위), `release_archive`(gzip 헤더
  + `tinfl`, ustar·GNU `L`·pax `path`, miniz ZIP 읽기, 최상위 폴더 하나 벗기기, 절대 경로·`..`·링크·장치
  거부, 512MB 상한, tar mode의 실행 비트), `update_install`(실행 파일 옆 `VERSION`과 쓰기 가능 여부로 설치
  조건, 파일마다 `.repiu-old`로 비키고 넣기, 실패 시 역순 되돌림, `.repiu-old-files` 목록과 다음 시작 때
  정리), `launcher_updater`(백그라운드 스레드: 확인 → Available → 받기 → 검증·풀기 → Staged, 취소,
  받은 바이트로 진행률).
* **플랫폼**: `platform/https_download.h`와 Linux(`curl`을 `posix_spawnp`로, 인자 배열, https만,
  `LD_PRELOAD`·`LD_LIBRARY_PATH` 제외, stderr를 파일로 받아 오류 문구에 씀, 취소 시 SIGTERM),
  Win32(WinHTTP, https→https 리다이렉트만, 읽기마다 취소·전체 시한 확인), web 스텁.
  `HostExecutablePath`(Linux `/proc/self/exe`, Win32 `GetModuleFileNameW`), `ReplaceProcessImage`
  (Linux `execv`), `WithdrawEnvironmentSetting`(`unsetenv` / `_putenv_s(name, "")`).
* **런처**: 맨 위 알림 줄(새 버전 + Update/Retry 버튼 또는 릴리스 주소와 이유, 진행 막대, 검증 중, 설치
  중), Staged가 되면 `install_update`로 창을 닫음. 설정 `[Launcher] check_updates`.
* **로더**: 런처 세션 시작에 `.repiu-old` 정리와 updater 생성(`check_updates`, `REPIU_UPDATE_CHECK`,
  시험용 `REPIU_UPDATE_CURRENT_VERSION`), 설치 후 이 세션이 게시한 런처 변수와 시험용 변수를 거두고
  다시 시작(Linux `execv`로 같은 PID, 실패하거나 Win32면 자식으로 실행해 그 종료 코드로 끝남).
* **probe**: `launcher_update`(core probe, 13개 항목, stored deflate로 만든 tar.gz·zip, 가짜 fetch).
* **CMake**: 새 소스, 플랫폼별 다운로드, Win32 `winhttp`.
* **문서**: 설계·작업 지시, 가이드(스팀덱 설치, 끄기, 문제 해결, 시험 절차), ARCHITECTURE, README,
  release-and-ci(아티팩트 이름·digest·`VERSION`에 기댄다는 점).

## 검증

이 환경은 Linux(Ubuntu 26.04.1, x11)라 Win32는 빌드하지 못했다.

| 검증 | 결과 |
|---|---|
| Linux x64·i386 Release 빌드 | 통과, 새 경고 없음 |
| core probe | 두 아키텍처 모두 `launcher_update_all=true`(13개 항목), 실패 0 |
| 실제 릴리스 아카이브 풀기 | v0.0.213 linux-x64 tar.gz(10개, 실행 비트 유지, 최상위 폴더 제거)와 win32 zip(9개, 루트 유지)을 새 코드로 풀어 Python `tarfile`/`zipfile` 결과와 SHA-256이 같음. 두 파일의 SHA-256은 GitHub `digest`와 같음 |
| 실제 업데이트(x64, 시험 폴더 `VERSION` 0.0.212, `REPIU_UPDATE_CURRENT_VERSION=0.0.212`) | 확인 0.35초 만에 `v0.0.213 available (installable)`, Update로 받기 0.5초, `staged 10 files`, `installed 10 files`, `execv`로 같은 프로세스가 v0.0.213 런처로 바뀌어 열림. 폴더의 `repiu`가 v0.0.213과 같은 해시, `VERSION` 0.0.213, 옛 파일 7개가 `.repiu-old`로 목록에 남음 |
| 정리 | 새 빌드를 다시 넣고 런처를 열자 `Removed 7 files left by the previous update`, 목록 파일도 지워짐 |
| 빌드 트리 보호 | `build/verify-x64/repiu`를 0.0.212로 속여 열면 `available (not a release install ...)`, `.repiu-update` 없음 |

## 확인하지 않은 것

* Win32 컴파일(WinHTTP·`GetModuleFileNameW`·`_putenv_s`)과 실제 교체·다시 시작. CI가 컴파일을 확인하고,
  실제 동작은 Windows에서 가이드 §5 절차로 확인해야 한다.
* 스팀덱 실기. 특히 게임 모드에서 `execv` 뒤 Steam이 계속 추적하는지는 같은 PID라는 근거로 기대할 뿐
  관찰하지 않았다.
* i386 빌드의 실제 업데이트(probe만), `curl`이 없는 Linux, 쓸 수 없는 설치 폴더의 실제 화면.
* 화면 표시(알림 줄·진행 막대)는 사용자가 Update를 눌러 동작했다는 로그로만 확인했다.

---

# Work log: launcher update check and in-place install (issue #48)

**Done.** The platform-neutral `repiu::update`: `semantic_version` (with or without `v`, numeric order,
`BuildVersionText`); `release_info` (a small depth-limited JSON parser reading `tag_name`, `html_url` and
each asset's name, size, URL and `sha256:` digest, plus the per-build asset name); `sha256` (FIPS 180-4,
whole files); `release_archive` (gzip header plus `tinfl`, ustar with GNU `L` and pax `path`, miniz's ZIP
reader, one top folder stripped, absolute paths, `..`, links and devices refused, a 512 MB cap, the
executable bit from the tar mode); `update_install` (the install condition from a `VERSION` next to the
executable and a writable folder, each file moved aside to `.repiu-old` before the new one goes in, reverse
rollback on failure, the `.repiu-old-files` list cleared at the next start); `launcher_updater` (a
background thread from check to Available to download to verify-and-unpack to Staged, cancellable, with
progress from the bytes received). Platform: `platform/https_download.h` with Linux (`curl` through
`posix_spawnp`, an argument array, https only, `LD_PRELOAD`/`LD_LIBRARY_PATH` dropped, stderr captured into
the error text, SIGTERM on cancel), Win32 (WinHTTP, https-to-https redirects only, cancel and the overall
limit checked per read) and a web stub; `HostExecutablePath` (Linux `/proc/self/exe`, Win32
`GetModuleFileNameW`), `ReplaceProcessImage` (Linux `execv`), `WithdrawEnvironmentSetting` (`unsetenv` /
`_putenv_s(name, "")`). The launcher draws a notice at the top (the newer version with Update/Retry, or the
releases address and why not, a progress bar, verifying, installing) and closes with `install_update` once
staged; `[Launcher] check_updates` stores the switch. The loader removes `.repiu-old` files and creates the
updater at the start of a launcher session (`check_updates`, `REPIU_UPDATE_CHECK`, the test-only
`REPIU_UPDATE_CURRENT_VERSION`), and after an install withdraws the launcher variables this session
published and the test variable, then restarts (Linux `execv` keeping the PID; on failure or on Win32 it runs
the new launcher as a child and exits with its code). A `launcher_update` core probe (13 checks, tar.gz and
zip built from stored deflate blocks, a fake fetch), CMake (new sources, per-platform download, `winhttp` on
Win32), and the docs: design, work order, a guide (Steam Deck install, switches, troubleshooting, a test
procedure), ARCHITECTURE, README, and release-and-ci (the names, digests and `VERSION` this relies on).

**Verified** on Linux (Ubuntu 26.04.1, x11); Win32 cannot be built here. Linux x64 and i386 Release builds
pass with no new warnings, and the core probe reports `launcher_update_all=true` (13 checks) and no failures
on both. The new code unpacked v0.0.213's real linux-x64 tar.gz (10 files, executable bits kept, top folder
stripped) and win32 zip (9 files at the root) with SHA-256s equal to Python's `tarfile`/`zipfile`, and both
archives' SHA-256s equal GitHub's `digest`. A real update on x64 (a test folder with `VERSION` 0.0.212 and
`REPIU_UPDATE_CURRENT_VERSION=0.0.212`): the check reported `v0.0.213 available (installable)` in 0.35 s,
Update downloaded in 0.5 s, `staged 10 files`, `installed 10 files`, and `execv` turned the same process
into the v0.0.213 launcher; the folder's `repiu` then had v0.0.213's hash, `VERSION` read 0.0.213, and seven
old files were listed as `.repiu-old`. With the new build put back, opening the launcher logged `Removed 7
files left by the previous update` and the list went too. Run from `build/verify-x64/` as 0.0.212, the
launcher reported `available (not a release install ...)` and created no `.repiu-update`.

**Not checked.** The Win32 compile (WinHTTP, `GetModuleFileNameW`, `_putenv_s`) and a real Win32 replacement
and restart: CI checks the compile, and the behaviour needs the guide's §5 procedure on Windows. A real Steam
Deck, in particular whether Steam's game mode keeps tracking the process after `execv` — expected from the
unchanged PID, not observed. A real update on the i386 build (probe only), a Linux without `curl`, and an
unwritable install folder on screen. The notice line and progress bar were confirmed only through the log
of the user pressing Update.
