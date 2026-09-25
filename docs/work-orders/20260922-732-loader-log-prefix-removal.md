# Task 732 작업 지시: 로더 로그의 `Win32` 접두어 제거

설계: [20260922-732](../design/20260922-732-loader-log-prefix-removal.md)

## 한국어

1. 로더 리터럴 시작의 `"Win32 ` 646개를 `"`로 치환.
2. 로더 리터럴에서 만든 판정 목록으로 `scripts/`, `docs/guides/`, `.github/`, `ARCHITECTURE.md`,
   `README.md`의 소비자 치환. 치환·보존 목록을 모두 검토.
3. 판정기가 처리하지 못한 중첩 괄호(`test_all.ps1` 282행)와 Task 731의 접두어 설명 문장 수동 수정.
4. 충돌 검사: 기존 로그 3개에서 패턴별 매칭 수 비교.
5. 검증: 두 host 빌드·core probe, `test_all.ps1` 단정의 이름 변경 전후 비교, WSL 실행.
6. 작업 로그.

하지 않을 것: 기록 문서와 `tests/history/` 수정, `exe_analyzer` 출력 변경, 로그 내용 변경.

---

## English

1. Replace the 646 `"Win32 ` literal starts in the loader with `"`.
2. Rewrite consumers in `scripts/`, `docs/guides/`, `.github/`, `ARCHITECTURE.md` and `README.md`
   from a decision list built from the loader literals, reviewing both the replaced and kept lists.
3. Fix by hand the nested parentheses the classifier cannot handle (`test_all.ps1` line 282) and
   Task 731's sentence about the prefix.
4. Collision check: compare per-pattern match counts on three existing logs.
5. Verification: both host builds and core probes, `test_all.ps1` assertions before and after the
   rename, and a WSL run.
6. Work log.

Not to be done: editing record documents or `tests/history/`, changing `exe_analyzer` output, or
changing log content.
