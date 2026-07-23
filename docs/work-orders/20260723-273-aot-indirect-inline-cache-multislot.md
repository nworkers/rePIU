# AOT 간접 call/jmp 다중 슬롯 인라인 캐시 작업 지시

## 한국어

### 작업 범위

1. `FF /2` near indirect call과 `FF /4` near indirect jump 방출기를 4개 entry chain으로
   확장합니다.
2. `AotIndirectInlineCacheSite` 설명을 반환 전용 표현에서 공통 다중 슬롯 표현으로
   갱신합니다.
3. 기존 Win32 patch worker의 target 재사용, 빈 슬롯 채움, round-robin 교체 정책을
   간접 call/jmp에도 적용합니다.
4. `aot_probe`에 call/jump layout과 guard chain을 확인하는 결정론적 검증을 추가합니다.
5. `ARCHITECTURE.md`, 관련 분석 문서와 작업 로그를 실제 결과로 갱신합니다.

### 비범위

- 원본 실행 파일 또는 게임 로직 수정
- DOS/DPMI/Glide HLE 의미 변경
- adaptive slot 수, LRU, 전역 간접분기 hash table
- 지원 범위를 벗어난 x86 encoding의 신규 번역

### 완료 조건

- 지원되는 indirect call/jump site가 4개 entry metadata와 올바른 compare chain을 방출
- Win32 patch/append/retire 경로가 모든 entry를 안전하게 처리
- `aot_probe` 결정론적 검증과 Win32 x86 Debug 빌드 성공
- 동일 시간 구동에서 indirect boundary miss 감소 및 의미 기반 실행 이정표 무회귀
- 설계·아키텍처·분석·작업 로그 갱신과 Git 커밋 완료

## English

### Scope

1. Extend `FF /2` near indirect call and `FF /4` near indirect jump emission to a
   four-entry chain.
2. Update `AotIndirectInlineCacheSite` documentation from return-only to shared
   multi-entry metadata.
3. Apply the existing Win32 refresh/fill/round-robin policy to indirect calls/jumps.
4. Add deterministic call/jump layout and guard-chain verification to `aot_probe`.
5. Update `ARCHITECTURE.md`, the relevant analysis, and the work log with results.

### Out of scope

- Original executable or gameplay-logic changes
- DOS/DPMI/Glide HLE semantic changes
- Adaptive entry counts, LRU, or a global indirect-branch hash table
- New translation support for currently unsupported x86 encodings

### Completion criteria

- Supported indirect call/jump sites emit four entries and a valid compare chain.
- Win32 patch, append, and retirement safely process every entry.
- Deterministic `aot_probe` verification and the Win32 x86 Debug build pass.
- Equal-duration runtime comparison reduces indirect misses without regressing semantic
  milestones.
- Design, architecture, analysis, work log, and Git commit are complete.
