# AOT 간접 call/jmp 다중 슬롯 인라인 캐시 작업 로그

## 한국어

### 결과

지원되는 prefix 없는 legacy-32 `FF /2` near indirect call과 `FF /4` near indirect
jump를 단일 슬롯에서 4슬롯 인라인 캐시로 확장했습니다. 원본 operand를 각 entry에서
비교하고, hit 시 기존 guest call-return ABI 또는 jump stack 의미를 유지하며 native
cache target으로 이동합니다. 원본 실행 파일, 게임 로직, DOS/DPMI/Glide HLE 의미는
변경하지 않았습니다.

### 변경 내용

- `src/runtime/aot_code_cache.cpp`
  - call/jump와 return이 공통 4-entry 상수를 사용합니다.
  - call/jump site가 4개의 compare/guard/hit block과 공통 miss tail을 방출합니다.
  - legacy offset 필드는 entry 0을 가리킵니다.
- `include/repiu/runtime/aot_code_cache.h`
  - `entries`를 call/jump/return 공통 다중 슬롯 메타데이터로 문서화했습니다.
- `src/platform/win32/aot_code_cache_win32.cpp`
  - 기존 다중 entry patch 정책의 적용 범위를 주석에 명확히 했습니다.
- `src/tools/aot_probe/inline_cache_probe.{h,cpp}`
  - synthetic call/jump layout, 네 target chain, 다섯 번째 target 교체, page retire
    guard reset을 실제 배치 cache byte로 검증합니다.
- `CMakeLists.txt`, `src/tools/aot_probe/main.cpp`
  - 새 probe를 `repiu_aot_probe`에 통합했습니다.
- `ARCHITECTURE.md`, 설계·분석·작업 문서
  - 4-entry layout, patch/coherence 계약, 실측 결과를 반영했습니다.

### 검증

1. `scripts/build_win32_x86.ps1`
   - Win32 x86 Debug 전체 빌드 성공.
2. `repiu_aot_probe build/runtime_mounts/pumpit1/PIU/PIU.EXE`
   - `inline_cache_call_layout=true`
   - `inline_cache_jump_layout=true`
   - `inline_cache_call_chain=true`
   - `inline_cache_jump_chain=true`
   - `inline_cache_round_robin=true`
   - `inline_cache_retirement=true`
   - `inline_cache_all=true`
   - `coherence_all=true`, 종료 코드 0
3. `REPIU_EXECUTION_BACKEND=aot-dynamic`, supervisor 120초
   - boundary reason `(ret/indir/direct/cond/other)` =
     `7,220 / 14,423 / 0 / 0 / 23,781`
   - 전체 boundary/reentry = `45,424 / 45,460`
   - Task 265 단일 슬롯 기준 `indir` 20,076 대비 5,653회, **28.2% 감소**
   - 전체 boundary 53,425 대비 8,001회, **15.0% 감소**
   - `_GRSSTWINOPEN@28` 약 14.5초 진입, logical window 약 15.5초 개방
   - fatal 0, supervisor timeout 종료(의도된 종료)

Task 265 기준과 이번 구동은 SDL3 전후의 서로 다른 build이므로 wall-clock 절대 A/B로
간주하지 않습니다. 최적화 대상인 `indir` 감소와 deterministic cache-byte 검증을 구현
효과의 근거로 사용합니다. 120초 내 texture/swap 이정표에는 도달하지 않았습니다.

## English

### Result

Extended supported prefix-free legacy-32 `FF /2` near indirect calls and `FF /4`
near indirect jumps from one entry to a four-entry inline cache. Each entry compares the
original operand, preserves the existing guest call-return ABI or jump stack semantics,
and transfers to the native cache target on a hit. Original executable bytes, gameplay
logic, and DOS/DPMI/Glide HLE semantics are unchanged.

### Changes

- The platform-neutral emitter now creates four call/jump compare/guard/hit blocks with
  a shared miss tail and shares the entry-count constant with returns.
- Metadata comments describe `entries` as the common call/jump/return representation.
- A dedicated `aot_probe` module validates layout, four-target chaining, fifth-target
  round-robin replacement, and page-retirement guard reset against placed cache bytes.
- CMake, probe integration, architecture, design, analysis, and task documentation were
  updated.

### Verification

- Full Win32 x86 Debug build passed.
- `repiu_aot_probe` reported every new `inline_cache_*` check and `coherence_all` as
  `true`, with exit code 0.
- A 120-second `aot-dynamic` run recorded boundary reasons
  `7,220 / 14,423 / 0 / 0 / 23,781`, total boundary/reentry `45,424 / 45,460`, and
  fatal count zero. Compared with the Task 265 single-entry reference, indirect misses
  fell by 5,653 (28.2%) and total boundaries by 8,001 (15.0%). The Glide gate was reached
  at about 14.5 seconds and the logical window opened at about 15.5 seconds.

The Task 265 reference predates the current SDL3 build, so this is not treated as a
controlled absolute wall-clock A/B. The targeted indirect-miss reduction and deterministic
cache-byte checks are the evidence for the implementation. Texture/swap was not reached
within this 120-second run.
