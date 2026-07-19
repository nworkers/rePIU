# AOT Probe 문자열 검색 작업 로그

## 결과

`repiu_aot_probe`에 `--findstr <text>` 읽기 전용 진단 모드를 반영했습니다. 재배치된 모든 런타임 객체를 검사하여 일치하는 ASCII 바이트열의 시작 가상 주소를 출력합니다.

## 검증

- `cmd /c scripts\\build_win32_x86.bat`: 성공
- `repiu_aot_probe <PIU.EXE> --findstr "Fatal error"`: 성공 (종료 코드 0)
- 관측 주소: `0x011A623C`, `0x011A626B`, `0x011A628D`

기존 빌드 경고 `execution_trampoline.cpp`의 C4819는 이번 변경과 무관하게 계속 관측되었습니다.

## 범위

문자열 리터럴 위치만 반환하며, 해당 문자열을 참조하는 명령어 또는 데이터 포인터를 추적하지는 않습니다.

---

# AOT Probe String-Search Work Log

## Result

Added the read-only `--findstr <text>` diagnostic mode to `repiu_aot_probe`. It scans every relocated runtime object and prints the virtual address at each matching ASCII byte sequence.

## Validation

- `cmd /c scripts\\build_win32_x86.bat`: passed
- `repiu_aot_probe <PIU.EXE> --findstr "Fatal error"`: passed (exit code 0)
- Observed addresses: `0x011A623C`, `0x011A626B`, `0x011A628D`

The pre-existing C4819 warning in `execution_trampoline.cpp` remains unrelated to this change.

## Scope

The mode returns literal locations only; it does not trace instructions or data pointers that reference a literal.
