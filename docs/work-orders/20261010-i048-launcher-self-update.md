# 작업 지시: 런처의 업데이트 확인과 제자리 설치 (issue #48)

설계: `docs/design/20261010-i048-launcher-self-update.md`

1. `repiu::update`: `semantic_version`, `release_info`(작은 JSON 파서, asset 선택), `sha256`,
   `release_archive`(tar.gz·zip, 경로 검사), `update_install`(설치 조건, 교체·되돌림, 정리),
   `launcher_updater`(백그라운드 상태 기계).
2. 플랫폼: `platform/https_download.h`와 Linux(`curl`)·Win32(WinHTTP)·web(스텁) 구현,
   `host_process.h`의 `HostExecutablePath`·`ReplaceProcessImage`.
3. 런처 설정 `[Launcher] check_updates`, 런처 UI의 알림 줄·Update 버튼·진행 막대.
4. 로더: 런처 세션에서 updater 생성(설정·환경 변수), `.repiu-old` 정리, 준비되면 설치와 다시 시작.
5. probe `launcher_update`(core probe), CMake(새 소스, Win32 `winhttp`).
6. 문서: ARCHITECTURE, README, 가이드(업데이트·스팀덱), 작업 로그.
7. 검증: Linux x64·i386 빌드와 core probe, 릴리스 아카이브 시험 폴더에서 실제 업데이트, Win32는 CI.

완료 기준: 릴리스 아카이브에서 푼 설치는 새 버전을 알리고 버튼 하나로 교체·다시 시작하며, 실패하면
기존 파일이 남는다. 빌드 트리는 덮어쓰지 않는다. 확인은 끌 수 있다.

---

# Work order: launcher update check and in-place install (issue #48)

(1) `repiu::update`: `semantic_version`, `release_info` (a small JSON parser and the asset choice),
`sha256`, `release_archive` (tar.gz and zip with path checks), `update_install` (install condition,
replace and roll back, cleanup), `launcher_updater` (the background state machine). (2) Platform:
`platform/https_download.h` with Linux (`curl`), Win32 (WinHTTP) and web (stub) backends, and
`HostExecutablePath` / `ReplaceProcessImage` in `host_process.h`. (3) The launcher setting
`[Launcher] check_updates`, and the launcher UI's notice line, Update button and progress bar. (4) Loader:
create the updater for a launcher session (setting and environment), clean up `.repiu-old`, and install and
restart once staged. (5) The `launcher_update` core probe; CMake (new sources, `winhttp` on Win32). (6)
Docs: ARCHITECTURE, README, a guide (updating, Steam Deck), the work log. (7) Verify: Linux x64 and i386
builds and the core probe, a real update in a test folder unpacked from a release archive, Win32 in CI.

Done when an install unpacked from a release archive announces a new version and replaces itself and
restarts with one button, a failure leaves the old files, a build tree is never overwritten, and the check
can be turned off.
