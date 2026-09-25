# Task 726 작업 지시: Linux x64 LFB native store census

## 한국어

### 작업 항목

1. LFB write lock의 32-bit guest range와 observer aggregate를 보관하는 공용 profile을 추가합니다.
2. 새 opt-in setting이 기존 native memory-write observer emission을 활성화하도록 연결합니다.
3. lock 성공 뒤 range를 시작하고 unlock의 기존 decode/present 전 range를 끝냅니다.
4. observer가 decoded explicit store만 range와 교차해 누적하도록 하며 legacy trace는 보존합니다.
5. snapshot/report와 Windows synthetic probe를 추가합니다.
6. Linux x64 및 Win32 x86 Debug build/core probe, 전용 probe, Linux bounded 관찰을 수행합니다.

### 완료 조건

- 기본 설정에서는 native observer emission과 LFB ABI가 변하지 않습니다.
- synthetic probe가 policy, intersection, lifecycle, malformed range를 통과합니다.
- Linux x64 bounded run의 counters와 관찰 범위를 작업 로그에 남깁니다.
- Win32 x86 구성을 실제 Debug build와 probe로 확인합니다.

### 범위 제외

- LFB readback 또는 present 최적화
- guest store 의미, guest register/flags, ABI 수정
- implicit/string/segment-override store의 완전한 계측 주장

---

## English

### Work items

1. Add a shared profile for the write lock's 32-bit guest range and observer aggregate.
2. Make the new opt-in setting enable existing native memory-write observer emission.
3. Start the range after successful lock output and end it before existing unlock decode/present.
4. Intersect only decoded explicit stores in the observer and preserve the legacy trace.
5. Add snapshot/reporting and a Windows synthetic probe.
6. Run Linux x64 and Win32 x86 Debug builds/core probes, the dedicated probe, and a bounded Linux observation.

### Completion criteria

- Default configuration leaves native observer emission and the LFB ABI unchanged.
- The synthetic probe passes policy, intersection, lifecycle, and malformed-range checks.
- The work log records bounded Linux counters and the evidence boundary.
- Win32 x86 is checked with an actual Debug build and probe.

### Out of scope

- LFB readback or presentation optimization
- Guest store semantics, guest registers/flags, or ABI changes
- Claims of complete coverage for implicit/string/segment-override stores
