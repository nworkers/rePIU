# 20260812-469 TargetProfile별 CAT702 capability 작업 로그 / Target-Scoped CAT702 Capability Work Log

- 설계: [20260812-469-target-scoped-cat702](../design/20260812-469-target-scoped-cat702.md)
- 작업 지시: [20260812-469-target-scoped-cat702](../work-orders/20260812-469-target-scoped-cat702.md)
- 분석: [cat702-piu-lock-check](../analysis/cat702-piu-lock-check.md)

## 한국어

### 수행 결과

- `TargetProfile`에 `enable_cat702`을 추가하고 PIU10 보드 capability와 독립적으로 Win32
  실행 준비 계층까지 전달했습니다.
- `Piu10IsaBoard`는 optional transform으로 초기화됩니다. transform이 없으면 보드와
  flash/MP3/DAC는 사용 가능하지만 CAT702 data-out은 0이고 직렬 입력은 무시됩니다.
- CAT702이 꺼진 profile은 `<target>.cat702`를 추출하거나 필수 자산으로 요구하지 않습니다.
- 기존 동작 보존을 위해 `pumpito`, `pumpitc`, `pumpitpc`, `pumpite`의 CAT702 기본값은
  true이고 다른 내장 profile은 false입니다.
- 로더 로그에 profile의 CAT702 활성 상태를 추가했습니다.

### 검증

- Win32 x86 Debug 최종 빌드: 성공
- `repiu_aot_probe.exe --piu10`: 성공
  - `piu10_cat702_vector=true`
  - `piu10_cat702_disabled=true,data-out=0,mp3-bytes=1`
- 전체 `pumpitpc` AOT probe: exit code 0
- `pumpitpc` profile의 CAT702을 일시적으로 false로 만든 실행:
  `CAT702 enabled: false` 확인, 약 19.6초 뒤 Lock Error의 키 대기인
  `unsupported DOS INT 21h AH=0x8`로 종료
- profile을 기본 true로 복원하고 재빌드한 최종 실행: 같은 오류 시점을 지나 34초 이상
  계속 실행되어 수동 종료
- 최초 빌드와 재시도가 겹쳐 한 번 `C1041` PDB 동시 쓰기 오류가 발생했으나, 중복 빌드가
  끝난 뒤 단일 빌드와 최종 복원 빌드는 모두 성공했습니다.
- `git diff --check`: 성공

## English

### Result

- Added `TargetProfile::enable_cat702` and carried it independently from the PIU10 board
  capability through Win32 execution setup.
- `Piu10IsaBoard` now initializes from an optional transform. Without one, the board and its
  flash/MP3/DAC remain available while CAT702 data-out is zero and serial input is ignored.
- A profile with CAT702 disabled neither extracts nor requires `<target>.cat702`.
- To preserve existing behavior, CAT702 defaults true for `pumpito`, `pumpitc`, `pumpitpc`, and
  `pumpite`, and false for all other built-in profiles.
- Added the profile's CAT702 state to loader logging.

### Verification

- Final Win32 x86 Debug build: passed
- `repiu_aot_probe.exe --piu10`: passed
  - `piu10_cat702_vector=true`
  - `piu10_cat702_disabled=true,data-out=0,mp3-bytes=1`
- Complete `pumpitpc` AOT probe: exit code 0
- Run with the `pumpitpc` CAT702 profile value temporarily false: logged
  `CAT702 enabled: false` and terminated after about 19.6 seconds at
  `unsupported DOS INT 21h AH=0x8`, the Lock Error key wait
- Final rebuilt run after restoring the default true value: continued past the same failure point
  for more than 34 seconds and was stopped manually
- One overlapping build retry produced an MSVC `C1041` concurrent-PDB error; the single build
  after the overlap and the final restored build both passed.
- `git diff --check`: passed
