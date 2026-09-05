# 작업 지시 20260905-595 — Linux x64 INT 31h 미지원 함수 HLE

설계: [20260905-595](../design/20260905-595-linux-x64-int31-unsupported-function-hle.md)

## 작업

1. `HandleDpmiInterrupt31`의 미지원 함수 분기를 표준 오류 반환 HLE로
   변경합니다.
2. `AX=8001h`, CF set, `EIP += 2` 및 `INT 31h` trace를 적용합니다.
3. 기존 DPMI 서비스 분기와 i386 실행 경로가 변하지 않는지 확인합니다.
4. Linux x64 `repiu`와 `repiu_core_probe`를 빌드하고 probe를 실행합니다.
5. watched `pumpit2a`를 실행하여 `AX=1E7F` trace와 다음 frontier를 기록합니다.
6. 분석 문서와 작업 로그를 갱신하고 하나의 Git 커밋으로 남깁니다.

## 완료 조건

* `INT 31h AX=1E7F`가 `[repiu-dos-int]`에 기록됩니다.
* 원본 guest `CD 31`을 Linux x64에서 실행하지 않고 `AX=8001h`, CF set으로
  다음 명령으로 진행합니다.
* Linux x64 core probe가 실패 없이 통과합니다.
* watched 실행에서 기존 `0x010F010C` raw SIGSEGV의 소멸 여부와 후속
  frontier가 작업 로그에 남습니다.

## English

# Work order 20260905-595 — Linux x64 INT 31h unsupported-function HLE

Design: [20260905-595](../design/20260905-595-linux-x64-int31-unsupported-function-hle.md)

## Work

1. Change the unsupported branch of `HandleDpmiInterrupt31` into a standard-error HLE response.
2. Apply `AX=8001h`, Carry Flag set, `EIP += 2`, and an `INT 31h` trace record.
3. Confirm that existing DPMI service branches and the i386 execution path are unchanged.
4. Build and run the Linux x64 `repiu` and `repiu_core_probe` targets.
5. Run watched `pumpit2a` and record the `AX=1E7F` trace and next frontier.
6. Update the analysis and work log, then leave one Git commit.

## Done when

* `INT 31h AX=1E7F` appears in `[repiu-dos-int]`.
* The original guest `CD 31` is not executed on Linux x64; execution advances
  to the next instruction with `AX=8001h` and Carry Flag set.
* The Linux x64 core probe passes without failures.
* The work log records whether the raw SIGSEGV at `0x010F010C` disappeared and identifies the next frontier.
