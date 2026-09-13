# Task 668 설계: DOS AH=43h 파일 속성 결과 추적

## 한국어

### 목적

Task 667은 `0x010EFEDC`의 일반적인 `MOV [EBP+0x18],EAX`가
`EAX=0`을 `0x0158C860`에 기록하는 사실을 확인했습니다. 정적 분석상 그
값은 앞선 `0x010F0B50` 계열 호출의 DOS `INT 21h AH=43h` 파일 속성 조회
결과에서 왔습니다. 기존 `REPIU_DOS_INT_TRACE`는 모든 DOS 호출의 진입을
출력하므로 반복 조회가 많은 실행에서는 원인을 읽기 어렵습니다.

이번 작업의 목적은 파일 속성 HLE의 공통 결과를 제한적으로 관찰하여, 실패한
guest 경로·DOS 가상 경로·host 경로·DOS 오류 코드·CF/EAX 결과를 확인하는
것입니다. 아직 파일 경로 규칙이나 HLE 의미론을 수정하지 않습니다.

### 설계 결정

새 opt-in 환경 변수 `REPIU_DOS_ATTR_TRACE=1`을 추가합니다. 공통
`HandleDosFileAttributes`가 `QueryDosFileAttributes` 또는
`SetDosFileAttributes`를 수행하고 결과 레지스터/CF를 확정한 뒤, 다음 정보를
bounded하게 출력합니다.

- guest EIP와 AH=43h subfunction
- guest path, 해석된 DOS virtual path, host path
- 성공 여부와 `DosPathResult` 오류 코드
- 반환 EAX, ECX, EDX, CF

성공 기록은 앞부분만 제한하고 실패 기록도 별도 카운터로 제한하여, hot loop가
로그를 무한히 증가시키지 않도록 합니다. guest 문자열을 읽지 못한 경우도
동일한 bounded trace로 구분합니다.

### 범위와 비범위

- 범위: 공통 DOS AH=43h HLE 결과의 opt-in provenance 진단
- 범위: 기존 `RecordDosPathTrace` 결과와 반환 레지스터의 상관관계 확인
- 비범위: 파일명 대소문자·확장자·VFS root 정책 변경
- 비범위: `0x010F0232`, `0x010EFEDC` 또는 특정 EIP/ESP 예외 처리
- 비범위: EAX=0을 유효한 반환 주소로 치환
- 비범위: DOS/HLE 서비스의 성공·실패 의미론 변경

### 검증 기준

`REPIU_DOS_ATTR_TRACE=1`을 켠 bounded `pumpit2a` 실행에서 반복 호출이
bounded 출력으로 제한되고, `AH=43h`의 실제 path/result/CF가 관찰되어야
합니다. 기존 Linux x64 빌드와 `repiu_core_probe`도 통과해야 하며, 진단을
끄면 실행 의미와 기존 오류 경계가 바뀌지 않아야 합니다.

## English

### Purpose

Task 667 established that the ordinary `MOV [EBP+0x18],EAX` at
`0x010EFEDC` stores `EAX=0` into `0x0158C860`. Static analysis ties that value
to the DOS `INT 21h AH=43h` file-attribute query in the preceding
`0x010F0B50`-family call. The existing `REPIU_DOS_INT_TRACE` prints every DOS
entry and becomes difficult to read when the query repeats in a hot loop.

This task observes the common file-attribute HLE result with a bounded trace so
the failing guest path, DOS virtual path, host path, DOS error code, and CF/EAX
result can be identified. It does not change path rules or HLE semantics.

### Design decision

Add the opt-in `REPIU_DOS_ATTR_TRACE=1` setting. After the shared
`HandleDosFileAttributes` completes `QueryDosFileAttributes` or
`SetDosFileAttributes` and establishes the result flags/registers, emit a bounded
record containing:

- guest EIP and the AH=43h subfunction;
- guest path, resolved DOS virtual path, and host path;
- success and `DosPathResult` error code;
- returned EAX, ECX, EDX, and CF.

Successful records are capped separately from failed records, so a hot loop cannot
grow the log without bound. A guest-string read failure is also reported through
the same bounded diagnostic.

### Scope and non-goals

- Scope: opt-in provenance for the common DOS AH=43h HLE result.
- Scope: correlate the existing `RecordDosPathTrace` observation with return
  registers.
- Non-goal: change filename case, extension, or VFS-root policy.
- Non-goal: add an exception for `0x010F0232`, `0x010EFEDC`, or any EIP/ESP.
- Non-goal: replace EAX=0 with a valid return address.
- Non-goal: change DOS/HLE service success or failure semantics.

### Verification criteria

With `REPIU_DOS_ATTR_TRACE=1`, a bounded `pumpit2a` run must show the actual
AH=43h path and result while keeping repeated output bounded. The Linux x64 build
and `repiu_core_probe` must continue to pass, and disabling the trace must leave
execution semantics and the existing failure boundary unchanged.
