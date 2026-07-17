# 작업 로그: DOS 파일 핸들 번호 재사용
# Work Log: DOS File Handle Number Recycling

관련: `docs/design/20260717-228-dos-file-handle-recycling.md`,
`docs/work-orders/20260717-228-dos-file-handle-recycling-order.md`

## 1. 근인 관측 경로 (How the root cause was found)

Task 227이 특성화만 하고 남긴 frontier(`0x030FAB04` fault, VA `0x00004091`)를
정적·동적 증거로 근인 확정했다. 코드 변경 전 조사:

* **정적 (repiu_aot_probe):** fault 블록 `0x030FAAF4`를 디스어셈블해
  `mov eax,[0x031A66FC]; shl edx,2; mov [edx+eax],ebx`(핸들 플래그 테이블 쓰기)임을
  확인. load 명령 바이트 `a1 fc661a01`은 **정확히** `[0x031A66FC]`를 읽는다(Task 226식
  명령 손상 아님). 베이스 포인터 정적값 `0x031A66AC`(probe `static_0x011A66FC=0x11a66ac`)
  는 자기 위치 `0x031A66FC`보다 **정확히 0x50(=엔트리 20개) 아래** → 테이블은 20칸이고
  `table[20]`의 주소가 곧 베이스 포인터 슬롯이다.
* **동적 (aot-dynamic pumpit1):** fault 재현 — `0xC0000005`, VA `0x00004091`, AOT
  매핑 → guest `0x030FAB04`. `handled DOS open/close = 16/16`,
  `last DOS open handle = 0x0014`(=20). DOS 파일 I/O 추적: 핸들이 `0x05→…→0x14`로
  단조 증가하고 각 핸들이 고립된 read/seek 블록으로 등장 → 파일을 순차적으로
  open/close하며 동시에 열린 파일은 1~2개뿐.

## 2. 근인 확정 (Root cause confirmed)

우리 HLE의 `OpenDosFile`이 `next_file_handle++`로 핸들을 단조 증가시키고
`CloseDosFile`이 번호를 회수하지 않아, 16번 순차 open이 핸들 20을 반환한다. 게스트
clib는 이 핸들을 20칸 테이블 인덱스로 써서 `table[20]`(= 베이스 포인터 슬롯
`0x031A66FC`)를 덮는다. 첫 오버플로우가 베이스 포인터를 `0x4041`(=플래그 `0x41` |
`0x4000`)로 손상시키고, 이후 접근이 `0x4041+0x50=0x4091`에서 fault한다. 실제 DOS는
가장 낮은 free 핸들을 반환하고 close 시 회수하므로 핸들이 5~6 범위를 벗어나지 않는다.
게스트가 아닌 **HLE의 DOS 핸들 시맨틱 위반**이 근인.

## 3. 수정 (Fix)

* `include/repiu/hle/dos_file_system.h`: 사용되지 않게 된 `next_file_handle` 필드 제거.
* `src/hle/dos_file_system.cpp`:
  - 상수 `kFirstDosUserHandle=5`, `kDosOpenHandleLimit=20` 추가.
  - `AllocateLowestFreeDosHandle`: `[5,20)`에서 현재 열려 있지 않은 가장 낮은 핸들 반환
    (모두 사용 중이면 0).
  - `OpenDosFile`: lowest-free 할당 + 닫힌 `open_files` 슬롯 재사용(벡터 무한 증가 억제).
    모두 사용 중이면 `kAccessDenied`("handle table is exhausted")로 실패(DOS "too many
    open files" 상당).

## 4. 검증 (Verification) — 통과

`build/win32_x86_dpmi`(VS 2026 번들 cmake, Win32)로 `repiu_loader_win32` 빌드 성공
(C4819 한글 주석 인코딩 경고만, 기존 소스와 동일 패턴, 무해).

* **aot-dynamic pumpit1 25초:**
  - `0x030FAB04`/VA `0x4091` 크래시 **소멸**.
  - `last DOS open handle = 0x0005`(핸들 재사용 정상), open/close `28/28`.
  - `dispatch_entry` **47,462 → 105,789**, `resize_cnt` 212(전체 시퀀스 완료), MSCDEX/CD
    서브시스템 진입(`mscdex_probe/request/cmd=1/1/3`). 회귀 없음(옛 크래시 지점 통과).
  - **새 frontier:** guest `0x030F4A98` `mov al,[ebx]`가 **EBX=0**(널 문자열 포인터)로
    `0xC0000005`(VA 0). 이 블록은 대소문자 무시 문자열 비교(stricmp류): `mov al,[ebx];
    mov ah,[edx]; A~Z면 +0x20; cmp al,ah`. fault 레지스터: EAX 0, EBX 0, ECX/ESI
    `0x0329B2B8`, EDX `0x03111043`(유효 소스), EDI `0x035D6BB4`, EBP `0x0329B2F8`,
    ESP `0x035D6BA8`. 별개 조사 대상.
* **trap 백엔드 30초 회귀:** `progress=612,186`(Task 226 기준선 118,504 대비 5배+),
  `dispatch_entry≈3,176,262`, `fatal_count=0`, 30초 타임아웃까지 크래시 없이 실행
  (`child_exit=124 terminated=true`). **회귀 없음, 대폭 전진.**

**단위 테스트:** 이 저장소에는 HLE용 단위 테스트 하네스가 없다(third_party libchdr만
테스트 보유). 프로젝트 규모의 테스트 프레임워크 도입은 이 작업 범위를 벗어나므로,
검증은 위 end-to-end 구동(핸들이 낮게 유지되고 특정 크래시가 소멸함을 직접 관측)으로
대체했다.

## 5. 후속 (Follow-up)

새 frontier `0x030F4A98`(널 문자열 포인터 stricmp)가 다음 조사 대상이다. EBX가 왜 0인지
— 어떤 lookup/파싱이 널을 반환해 이 비교로 전달되는지 역추적이 필요하다. EDX
(`0x03111043`)가 가리키는 문자열과 호출 컨텍스트가 단서.

---

**English summary.** Root-caused and fixed the Task 227 frontier (`0x030FAB04`, fault VA
`0x00004091`). Static `repiu_aot_probe` disassembly confirmed the block writes a handle-flags
table `table[handle] = value|0x4000` with base `[0x031A66FC]`, whose static value
`0x031A66AC` sits exactly `0x50` (20 four-byte entries) below the base-pointer slot — a
20-entry table with a self-pointing base immediately after. Dynamic aot-dynamic traces showed
the game opens/closes 16 files sequentially (≤2 open at once), yet our HLE allocated handles
via monotonic `next_file_handle++` and never recycled numbers on close, so the 16th open
returned handle 20, overflowing the table and corrupting the base to `0x4041`; the next access
faulted at `0x4091`. Fixed `OpenDosFile` to allocate the lowest free handle in `[5,20)` with
closed-slot reuse (removing `next_file_handle`). Verified: the crash is gone, handles stay at
`0x05`, aot-dynamic advances dispatch 47,462→105,789 (resize 212, into MSCDEX/CD) to a new
frontier at `0x030F4A98` (`mov al,[ebx]` with EBX=0, a null string pointer in a case-insensitive
compare), and the trap backend advances 5x (progress 612,186) with no fatal over 30 s. No unit
test harness exists in this repo, so verification is via the end-to-end runs.
