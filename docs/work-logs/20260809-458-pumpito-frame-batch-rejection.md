# 20260809-458 pumpito frame batch 거부 진단 작업 로그 / Pumpito Frame-Batch Rejection Diagnostics Work Log

설계: [20260809-458-pumpito-frame-batch-rejection.md](../design/20260809-458-pumpito-frame-batch-rejection.md)
작업 지시: [20260809-458-pumpito-frame-batch-rejection.md](../work-orders/20260809-458-pumpito-frame-batch-rejection.md)

## 한국어

### 결과

- 실제 실행의 최초 거부 사유를 `relocation`으로 확인했습니다.
- 원인은 LE fixup target offset을 전체 image 상대 주소로 해석한 Task 457의 주소 모델이었습니다.
- target object 4의 relocated base를 실행 context에 보존하고 모든 MP3 loop data 주소를
  `object 4 base + fixup target offset`으로 계산하도록 교정했습니다.
- synthetic probe를 실제 object-relative 배치로 변경했습니다.
- batch span은 논리 `0xE00`까지만 채워 물리 ring의 race headroom을 보존합니다.
- 정상 frame/file 경계인 `count==target`, `cursor==end`는 거부 진단으로 출력하지 않습니다.

### 검증

- Win32 x86 Debug `repiu`, `repiu_aot_probe` build: 성공
- `repiu_aot_probe.exe --piu10`: 성공
- 교정 직후 실제 실행: `verified frame-tail batch active` 확인
- 최종 45초 제한 실행:
  `received/dropped/decoded/pcm/starved/batched/ring-high=123943/0/297/342144/6/123372/3638`
- 기존 사용자 실행은 동일 구간에서 `722248/0/1728/1990656/45/0/58`이었으므로 batch
  활성화와 starvation 감소가 확인되었습니다.

### 남은 검증

사용자 환경에서 음악 중 렌더링·입력·장면 전환이 계속되는지 확인해야 합니다.

## English

### Result

- The first real-runtime rejection was confirmed as `relocation`.
- Task 457 incorrectly treated LE fixup target offsets as whole-image-relative addresses.
- Execution context now retains the relocated base of target object 4, and all MP3 loop data
  addresses use `object 4 base + fixup target offset`.
- The synthetic probe now reproduces the real object-relative layout.
- Batch spans stop at logical `0xE00`, preserving physical ring race headroom.
- Normal frame/file boundaries `count==target` and `cursor==end` no longer report rejection.

### Verification

- Win32 x86 Debug `repiu` and `repiu_aot_probe` builds: passed
- `repiu_aot_probe.exe --piu10`: passed
- First corrected runtime: observed `verified frame-tail batch active`
- Final bounded 45-second run:
  `received/dropped/decoded/pcm/starved/batched/ring-high=123943/0/297/342144/6/123372/3638`
- The comparable user run was `722248/0/1728/1990656/45/0/58`, confirming batch activation
  and reduced starvation.

### Remaining Validation

Confirm in the user's environment that rendering, input, and scene transitions continue during
music playback.
