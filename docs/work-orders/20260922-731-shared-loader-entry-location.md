# Task 731 작업 지시: 공용 로더 entry point 이동

설계: [20260922-731](../design/20260922-731-shared-loader-entry-location.md)

## 한국어

1. `git mv src/host/win32/main.cpp src/host/loader/main.cpp` (내용 변경 없음).
2. `CMakeLists.txt`: Linux·Win32 두 `repiu` target의 소스 경로와 Task 503d-17 주석 갱신.
3. 문서: `ARCHITECTURE.md`의 디렉터리 설명과 entry point 언급, `README.md` 디렉터리 표.
4. 검증: Win32 전체 빌드·core probe, WSL Linux x64 빌드·core probe·짧은 실행, rename 100%.
5. 작업 로그.

하지 않을 것: 파일 내용 수정, `"Win32 ..."` 로그 접두어 변경, `supervisor_main.cpp` 이동.

---

## English

1. `git mv src/host/win32/main.cpp src/host/loader/main.cpp`, with no content change.
2. `CMakeLists.txt`: update the source path of both `repiu` targets (Linux and Win32) and the
   Task 503d-17 comment.
3. Documentation: the directory description and entry-point mentions in `ARCHITECTURE.md`, and the
   directory table in `README.md`.
4. Verification: Win32 full build and core probe; WSL Linux x64 build, core probe and a short run;
   100% rename.
5. Work log.

Not to be done: changing the file's contents, changing the `"Win32 ..."` log prefix, or moving
`supervisor_main.cpp`.
