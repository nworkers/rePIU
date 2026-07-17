# 작업 로그: LE cross-page fixup 부호 확장 수정
# Work Log: LE Cross-Page Fixup Sign-Extension Fix

## 1. 근인 관측 경로 (How the root cause was found)

Tasks 223-225에서 int3 sentinel(문제의 호출에서 재발화 안 함)·워치포인트(ESP 근접
불가)·동시 writer(전부 배제)를 소진한 뒤, **trap 백엔드 full 단일스텝**으로 손상
구간 `[0x21F36, 0x21F90]`에서 `[esp+0x154]`를 명령별 캡처했다(40분 예산, resize
212에서 fault 도달, 트레이스 덤프). 결과:

* 1차 루프 반복: store 직후 `[esp+0x154]=0x0325E1F8`(정상), load도 정상.
* **2차 반복은 `0x21F63`에서 시작**(setup/store 미실행) 하고 이미
  `[esp+0x154]=0xDD1523B1`. 즉 `0x21F63`은 setup 이후 반복되는 **루프 본체**였다.

`repiu_aot_probe` 디스어셈블로 루프 back-edge(`jmp 0x21F63` @ `0x2204D`)와 포인터
전진 코드를 확인, `0x21FFD: mov edx,[esp+0x11A8A]`가 프레임(0x190) 밖 72KB를 읽어
`0xDD152395+0x1C=0xDD1523B1`을 만든다는 것을 특정했다. 짝이 되는 store
`0x2202D: mov [esp+0x154],edx`는 정상 disp(0x154)라 **load만 손상**됨을 확인.

## 2. 근인 확정 (Root cause confirmed)

* 디스크 원본 바이트(LE-loaded object memory에서 인접 명령
  `8b 9c 24 44 01 00 00`을 검색해 앞 명령 추출): `8b 94 24 54 01 00 00`
  = `mov edx,[esp+0x154]` (정상).
* 로더 처리 후: `8b 94 e4 8a 1a 01 00` = `mov edx,[esp+0x11A8A]` (손상).
* 임시 진단(aot_probe에 fixup 레코드 덤프)으로 손상 위치 object off `0x11FFF`
  (= guest `0x21FFF`, 페이지 경계)에 fixup `page_index=0x3, source_offset=0xFFFF,
  target_object=4`가 적용됨을 확인. `0xFFFF`는 부호 있는 `-1`(cross-page 표식).

## 3. 수정 (Fix)

`source_offset`을 `int16_t`로 부호 확장:
* `src/exe/executable_headers.cpp` `ApplyLeInternalRelocations`
* `src/runtime/runtime_memory.cpp` `FindSourceObjectForPage`(런타임 이미지 재적용
  공유 헬퍼 — 같은 버그가 복제되어 있었고, 이걸 고치기 전엔 aot_probe/런타임
  이미지가 여전히 손상이었다)

음수 결과는 스킵(경계 방어). 임시 진단 코드는 되돌렸다.

## 4. 검증 (Verification) — 통과

* `repiu_aot_probe <PIU.EXE> 0x01021FFD` → `mov edx, [esp+0x154]` (복원 확인).
* `repiu_supervisor_win32 pumpit1` aot-dynamic 30초:
  - `0xDD1523B1` crash **소멸** (EDI가 `0xDD1523B1`→`0x00000001`).
  - `dispatch_entry` **63446→95867**로 전진(이전 크래시 지점 통과 = 회귀 없음).
  - 새 frontier: `0x030F7A0C`, fault VA `0x00004091`(저지연 주소) — 별개 문제.
* 빌드: `build/win32_x86_dpmi`(VS 2026 Community 번들 cmake)로 loader/supervisor/
  aot_probe 정상 빌드.

## 5. 후속 (Follow-up)

새 frontier `0x030F7A0C`(fault VA `0x4091`, EAX `0x4041`/EBX `0x4000`)는 저지연
메모리 접근 계열로 보이며 별도 조사가 필요하다. 또한 이 부호 확장 버그는
cross-page fixup 전반에 영향을 주므로, 다른 위치의 유사 손상도 함께 해소됐을 수
있다(실행이 더 멀리 전진한 것이 방증).

---

**English summary.** Root-caused the Tasks 221-225 frontier via a 40-minute trap-backend
full single-step trace over `[0x21F36, 0x21F90]`: the crashing execution is the **second
iteration** of a loop (body `0x03021F63`) that advances a filename destination pointer
`[esp+0x154]` by `0x1C` per iteration; the load `mov edx,[esp+0x154]` at guest
`0x03021FFD` was corrupted by the loader into `mov edx,[esp+0x11A8A]`, reading 72 KiB
outside the 0x190 frame and producing the wild constant. Confirmed by comparing the raw
disk bytes (`8b 94 24 54 01 00 00`) with the loader-processed bytes
(`8b 94 e4 8a 1a 01 00`) and by a temporary aot_probe fixup dump showing a
`source_offset=0xFFFF` (signed -1) cross-page fixup landing at the instruction. Fix:
sign-extend `source_offset` (`int16_t`) in `ApplyLeInternalRelocations`
(`executable_headers.cpp`) and the runtime helper `FindSourceObjectForPage`
(`runtime_memory.cpp`) — both had the unsigned bug; fixing only the first left the runtime
image still corrupted. Verified: `repiu_aot_probe` shows the instruction restored to
`mov edx,[esp+0x154]`; the `0xDD1523B1` crash is gone (EDI now `0x1`) and execution
advances (dispatch 63446→95867) to a new, unrelated frontier at `0x030F7A0C` (fault VA
`0x4091`). No regression (execution passed the old crash site).
