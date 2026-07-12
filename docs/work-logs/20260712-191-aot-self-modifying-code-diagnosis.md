# AOT self-modifying code 진단 작업 로그

## 관찰

Win32 x86 build와 AOT probe를 사용해 GETPROCADDR continuation
`0x030F3418`, patch store `0x030F342C/0x030F3432`, indirect jump
`0x030F3436`, import stub `0x030FED0E`를 연결했습니다.

10초 `aot-dynamic` 실행은 다음을 기록했습니다.

* exception/fallback 없음
* inline-cache patch `1,122/1,122`
* LINEXE bridge entry `19,612`, GETPROCADDR `19,611`
* 마지막 export `_GRGLIDEINIT@0`, result `0x045D0300`
* Glide gate entry `0`
* 마지막 indirect source/target `0x030F3436 -> 0x030FED0E`

정적 stub은 resolver call이며 guest는 이를 `E9 rel32`로 바꿉니다. AOT가 동일
guest address의 변경 전 cache entry를 계속 선택하는 것이 반복의 직접 원인임을
확인했습니다.

## 구현 및 검증

선택한 generation-first 정책에 따라 page provenance, active/generation state,
worker-owned retirement, write watch, lazy live retranslation, stale entry relink와
page-local quarantine을 구현했습니다. 정상 hot path의 전체-map scan과 동적 append의
`new × previous` 재연결 탐색을 제거하고 inactive index만 조회하도록 바꿨습니다.

결정론적 AOT probe는 다음 항목을 모두 통과했습니다.

* retirement와 active lookup 제거
* cache-to-guest provenance 유지
* 수정된 live immediate의 generation 2 반영
* 오래된 5-byte entry의 `E9` relink
* 새 generation 재폐기
* HLE excluded range의 `INT3` boundary 생성

PIU 관찰에서 code write/retire/publish/relink는 `2/1/1/2`, 실패와 quarantine은
`0/0`, GETPROCADDR는 `1`이었습니다. 합성 Glide gate를 일반 CFG로 복사해 발생한
`UD2` 예외는 HLE 소유 범위를 platform-neutral planner에서 제외해 해결했습니다.
최종 10초 실행은 Glide ordinal `0x20`, `0x2D`를 통과하고 heartbeat 약 228만에서
supervisor가 정상적으로 종료했습니다.

세대 정책의 구조, 성능 회귀 분석, 검증 범위와 알려진 한계는
[AOT self-modifying page 일관성 작업 로그](20260712-191-aot-self-modifying-page-coherence.md)에
정리했습니다.

검증 명령:

```powershell
cmd /c scripts\build_win32_x86.bat
build\win32_x86_debug\Debug\repiu_aot_probe.exe build\runtime_mounts\pumpit1\PIU\PIU.EXE
$env:REPIU_EXECUTION_BACKEND='aot-dynamic'
build\win32_x86_debug\Debug\repiu_supervisor_win32.exe pumpit1 10000
```

# AOT Self-modifying Code Diagnosis Work Log

Differential disassembly connected the successful LINEXE result, the two guest
stores that patch `0x030FED0E`, and the following jump back to that address. A
ten-second AOT run completed 19,611 GETPROCADDR calls but entered no Glide gate.
The stale pre-patch cache entry is the direct cause. The selected implementation
uses generation retranslation first and page-local quarantine as its fail-closed
fallback.

## Implementation and Verification

Implemented the selected generation-first policy with page provenance,
worker-owned retirement, write watches, lazy live retranslation, stale-entry
relinking, and page-local quarantine. The deterministic probe passed retirement,
provenance, live snapshot, generation-2 publication, relinking, repeated
retirement, and excluded-HLE-boundary checks. PIU recorded code
writes/retirement/publication/relinks of `2/1/1/2`, one GETPROCADDR call, and no
generation failure or quarantine. Excluding the synthetic Glide gate from the
platform-neutral CFG fixed the copied-`UD2` exception. The final bounded
ten-second run reached Glide ordinals `0x20` and `0x2D` and continued until the
supervisor timeout at roughly 2.28 million heartbeats.

The generation architecture, performance-regression analysis, verification scope,
and known limits are consolidated in the
[AOT self-modifying page coherency work log](20260712-191-aot-self-modifying-page-coherence.md).
