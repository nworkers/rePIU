# AOT Probe 문자열 검색 작업 지시

## 목표

재배치된 실행 이미지 안에서 지정 문자열을 찾는 `repiu_aot_probe --findstr` 진단 경로를 추가하고, 실제 PIU 실행 파일로 검증합니다.

## 구현 계획

1. 모든 재배치 객체를 순회하는 읽기 전용 바이트열 검색 함수를 추가합니다.
2. `--findstr <text>` 명령줄 모드와 사용법을 추가합니다.
3. Win32 x86 Debug를 빌드하고 알려진 오류 문자열 검색으로 출력과 종료 상태를 확인합니다.

## 완료 기준

- 기존 프로브 모드의 인수 형식을 유지합니다.
- 검색 결과가 재배치 후 런타임 가상 주소로 출력됩니다.
- Win32 x86 Debug 빌드가 성공합니다.

---

# AOT Probe String-Search Work Order

## Goal

Add a `repiu_aot_probe --findstr` diagnostic path that finds a supplied string in the relocated executable image, then validate it with the real PIU executable.

## Implementation plan

1. Add a read-only byte-sequence search over every relocated object.
2. Add the `--findstr <text>` command-line mode and usage text.
3. Build Win32 x86 Debug and check output and exit status with a known error-string search.

## Acceptance criteria

- Existing probe-mode argument forms remain intact.
- Results print relocated runtime virtual addresses.
- The Win32 x86 Debug build succeeds.
