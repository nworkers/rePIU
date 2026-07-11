# DOS/16M resident table 정적 복원 작업 로그

## 결과

* DOS4GW.EXE의 MZ relocation 78개를 entry 위치, target segment:offset, load-image/file offset, 원본 word와 함께 복원했다.
* 다섯 BW module의 header/GDT를 순회해 selector copy record 16개를 복원했다.
* RSI-2 block 11개와 relocation offset 1,110개를 누락 없이 복원했다.
* 모든 항목을 deterministic JSON manifest로 저장하고 사람이 읽는 분석 문서와 Mermaid 도식을 추가했다.
* Open Watcom `exe16m.h`와 `load16m.c`는 포맷 근거로만 참고했으며 코드는 사용하지 않았다.

| Module | Copies | RSI-2 blocks | Relocations |
| --- | ---: | ---: | ---: |
| EXPLOAD | 3 | 2 | 160 |
| LINEXE | 3 | 2 | 236 |
| INT31W | 3 | 2 | 163 |
| WVMM | 3 | 2 | 275 |
| DOS4GW | 4 | 3 | 276 |

## 검증

* manifest를 임시 파일로 재생성하고 SHA-256 동일성을 확인했다.
* 첫 BW signature를 손상한 in-memory 입력이 `missing BW signature at 0xF474`로 거부됨을 확인했다.
* `git diff --check`를 통과했다.
* `cmake --build build\win32_x86_debug --config Debug`가 성공해 library, analyzer, loader, supervisor를 모두 빌드했다.

## 다음 의사결정 지점

정적 table 자체는 전부 복원됐다. 최종 `CS:[0x066A]` service-zero target을 얻으려면 다음 작업에서 두 접근 중 하나를 선택해야 한다.

1. 현재 manifest를 입력으로 DOS/16M selector/base 할당과 relocation 적용을 symbolic replay한다.
2. 실제 DOS/16M 실행을 계측하여 할당 결과만 캡처하고 manifest와 대조한다.

사용자가 정적 경로를 선택했으므로 일관된 다음 단계는 1번이다. 다만 이는 table parser가 아니라 loader의 동적 배치 정책을 모델링하는 별도 구현 단위다.

# DOS/16M Resident Table Static Reconstruction Work Log

Recovered all 78 resident MZ relocations, 16 selector copy records, and 1,110 RSI-2 relocation offsets in 11 blocks across all five BW modules. A deterministic 64-KiB JSON manifest preserves every entry. Regeneration was byte-identical, malformed BW input was rejected, `git diff --check` passed, and all Win32 x86 Debug targets built successfully.

The static tables are complete. The next decision is symbolic replay of DOS/16M selector/base allocation versus runtime capture. Since the static path was selected, symbolic replay is the consistent next task, but it is a separate dynamic-placement model rather than further table parsing.
