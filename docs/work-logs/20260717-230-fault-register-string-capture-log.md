# 작업 로그: fault 시점 레지스터 문자열 캡처 진단 + Task 229 심화
# Work Log: Fault-time register string capture diagnostic + Task 229 deepening

관련: `docs/work-orders/20260717-230-fault-register-string-capture-order.md`,
`docs/analysis/current-execution-frontier.md` Task 229/230 절.

## 1. 구현 (Diagnostic implementation)

fault 시점 각 GPR(EAX/EBX/ECX/EDX/ESI/EDI)이 가리키는 게스트 메모리의 ASCII 문자열을
최대 32바이트 캡처·보고하는 재사용 가능한 진단을 추가했다.

* `include/repiu/platform/win32/execution_trampoline.h`,
  `src/platform/win32/execution_trampoline.cpp`:
  attempt/ThreadContext에 `exception_register_strings[6][32]` +
  `exception_register_string_valid_mask` 추가. `CaptureException`에서 각 레지스터 값을
  시작 주소로 `ReadProcessMemory`(첫 바이트 읽히면 valid 비트). context→attempt 미러링
  추가.
* `src/host/win32/main.cpp`: 리포트에 각 레지스터 문자열 출력(비인쇄 문자는 '.'로
  치환, 원본 hex 병기).

빌드: `build/win32_x86_dpmi` loader 성공(C4819 한글 주석 경고만).

## 2. 관측 결과 (Findings — Task 229 근인 심화)

`aot-dynamic pumpit1` 구동으로 Task 229 fault 재현 시 새 진단 출력:

* **EDX 문자열** = `"tga.pcx.ptx.rgb..rt.T.TYPE.NUM.T"` — 확장자 비교 리터럴(예상대로).
* **ESI 문자열(0x0329B2B8) = 전부 0(빈 문자열)**, `[ESI+0x20..0x3C]`도 전부 0. 즉
  파싱 대상 소스 파일명은 **확장자 없는 파일명이 아니라 완전히 빈(미초기화) 버퍼**다.

이어 정적 분석으로 콜스택을 3단계까지 좁혔다:

1. **stricmp** `0x030F4A94`(`stricmp(EAX,EDX)`): `mov al,[ebx]` EBX=0 → fault.
2. **parse_texture** `0x03019C78`(`parse_texture(EAX=struct, EDX=filename)`,
   프레임 0x11C): arg2(EDX)=filename=`0x0329B2B8`(빈 버퍼)를 로컬로 복사→
   strtok(".")→strtok(" .")→stricmp(확장자, "tga"/"pcx"/"ptx"/"rgb"). 첫 strtok가
   빈 문자열이라 이후 strtok가 NULL→stricmp null-deref. `mov esi,edx`로 esi가 곧
   filename이고 strtok/`0x03025294`가 esi 보존하므로 fault 시 ESI=filename 확정.
3. **호출자 loop** `0x0301B015: call 0x03019C78`: **텍스처 descriptor 배열을 고정 40회
   순회**(`esi` 0→0xA0 step 4, 엔트리 stride `0x6C`=108). 각 엔트리 포인터(ECX)를
   filename으로 parse_texture에 전달. 크래시 엔트리(0x0329B2B8)는 **전부 0(미기록)**.

**GAMEVIEW.BGA 확인:** 크래시 직전 마지막 open은 `DATAS\BGA\GAMEVIEW.BGA`이나, 이
파일엔 텍스처 파일명 문자열이 **전혀 없다**(헤더 "BGA"+0x78, 이후 0, 그다음 float
`0x3F800000`=1.0f 데이터). 반면 `MODEL\T05..T19.3DM` 모델 파일들은 확장자 없는 텍스처
베이스명("p_t0","b_w")을 담는다(read 프리픽스로 확인).

**근인 방향:** 40칸 텍스처 descriptor 배열의 한 엔트리가 채워지지 않아 빈 파일명이
parse_texture로 전달됨. 실제 DOS라면 이 배열이 전부 채워져 있었을 것(빈 엔트리면 DOS
에서도 stricmp가 null-deref). **다음: 이 배열을 채우는 상위 코드(배열 base·채우는
루프)를 추적해 왜 한 엔트리가 비는지 규명.**

## 3. 검증 / Verification

진단 자체는 fault 리포트에 레지스터 문자열이 정상 출력됨으로 검증(위 EDX="tga..." 등).
게임 실행 동작에는 영향 없음(예외 캡처 경로에만 읽기 추가). 근인 확정·수정은 후속.

---

**English summary.** Added a reusable fault-time diagnostic that captures up to 32 ASCII bytes
at each GPR (EAX/EBX/ECX/EDX/ESI/EDI) in `CaptureException` and prints them in the report. It
immediately refined the Task 229 frontier: the source string at ESI (`0x0329B2B8`) is **all
zeros — an empty/uninitialized buffer**, not an extension-less filename (EDX correctly shows the
literal `"tga.pcx.ptx.rgb..rt..."`). Static analysis then narrowed the call stack three levels:
`stricmp(0x030F4A94)` ← `parse_texture(0x03019C78)` (arg2 = the empty filename buffer) ← a loop
at `0x0301B015` that iterates a fixed 40 times over a **texture-descriptor array** (108-byte
stride) passing each entry as the filename; the crashing entry (`0x0329B2B8`) is entirely zero
(never written). GAMEVIEW.BGA (the last file opened) contains no texture filenames at all
(header "BGA"+0x78, then zeros, then 1.0f floats), while the `MODEL\T*.3DM` files carry
extension-less texture base names ("p_t0","b_w"). Root cause direction: one entry of the 40-slot
texture-descriptor array is not populated, so an empty filename reaches parse_texture (which,
like on real DOS, null-derefs in stricmp). Next: trace the upper code that fills the array to
find why an entry stays empty.
