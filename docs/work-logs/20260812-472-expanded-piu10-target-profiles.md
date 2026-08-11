# 작업 로그: PIU10 타깃 프로파일 추가

## 결과

기존 `pumpite`를 중복하지 않고 그 뒤에 `pumpitpr`, `pumpitpx`, `pumpit8`, `pumpitp2`,
`pumpipx2`, `pumpitp3`, `pumpipx3`을 요청 순서대로 추가했습니다.

각 프로파일은 short name을 `id`와 `rom_set_id`로 사용하고, 공용
`build/runtime_mounts/<id>/PIU/PIU.EXE`, `piu_common`, runtime reservation을 사용합니다.
MAME 공식 드라이버의 PIU10 BIOS/CAT702 정의에 따라 JAMMA, PIU10 ISA, CAT702 capability를
모두 활성화했고 MP3 latency는 0 ms입니다.

## 검증

- Win32 x86 Debug `repiu`, `repiu_aot_probe`, `repiu_exe_analyzer`: 성공
- Win32 x86 Release 동일 target: 성공
- Debug/Release profile probe:
  - `piu10_target_profiles=true`
  - `jamma_target_profiles=true`
  - `piu10_mp3_latency_profile=true,milliseconds=0`
- 신규 7개 analyzer 호출:
  - 모두 unknown profile 없이 해당 id의 ZIP 검증을 통과
  - 모두 `<id> CHD directory not found`로 종료

현재 신규 세트의 CHD가 없으므로 mount와 guest 실행은 검증하지 않았습니다. 기존 C4819와
Release probe LNK4217 경고 외에 새 빌드 오류는 없습니다.

README의 지원 목록을 갱신하면서 Task 466 이후에도 남아 있던 `pumpito` 기본 50 ms 설명을
현재 구현과 같은 모든 PIU10 profile 기본 0 ms로 바로잡았습니다.

---

# Work log: expanded PIU10 target profiles

## Result

Without duplicating the existing `pumpite`, added `pumpitpr`, `pumpitpx`, `pumpit8`, `pumpitp2`,
`pumpipx2`, `pumpitp3`, and `pumpipx3` immediately after it in the requested order.

Each profile uses its short name as both `id` and `rom_set_id`, with the shared
`build/runtime_mounts/<id>/PIU/PIU.EXE` path, `piu_common`, and runtime reservation. JAMMA, PIU10
ISA, and CAT702 capabilities are enabled from MAME's official PIU10 BIOS/CAT702 definitions, and
MP3 latency defaults to zero milliseconds.

## Verification

- Win32 x86 Debug `repiu`, `repiu_aot_probe`, and `repiu_exe_analyzer`: passed
- Same Win32 x86 Release targets: passed
- Debug/Release profile probe:
  - `piu10_target_profiles=true`
  - `jamma_target_profiles=true`
  - `piu10_mp3_latency_profile=true,milliseconds=0`
- Analyzer invocation for all seven new ids:
  - each passed ZIP validation without an unknown-profile result
  - each ended with `<id> CHD directory not found`

Mounting and guest execution remain unverified because the new sets currently have no local CHDs.
There were no new build errors beyond the existing C4819 and Release-probe LNK4217 warnings.

While updating the README support list, the stale statement that `pumpito` defaults to 50 ms was
corrected to the current Task 466 behavior: every PIU10 profile defaults to zero milliseconds.
