# 20260810-465 공용 PIU10 port-output batching 작업 로그 / Generic PIU10 Port-Output Batching Work Log

설계: [20260810-465-generic-piu10-port-output-batching.md](../design/20260810-465-generic-piu10-port-output-batching.md)

작업 지시: [20260810-465-generic-piu10-port-output-batching.md](../work-orders/20260810-465-generic-piu10-port-output-batching.md)

## 한국어

### 구현 결과

- `pumpito` ROM-set gate와 고정 EIP, LE object 4, `0x3434xx` offset을 제거했습니다.
- current `OUT DX,AL` 주변 feeder-loop shape에서 모든 state 주소와 service 경계를 추출합니다.
- cursor/count operand alias, branch target, guest range와 frame state를 모두 fail-closed로
  검증합니다.
- PIU10 capability가 활성화된 모든 target에서 batch를 시도하며, 불일치는 기존 scalar
  byte path를 유지합니다.
- MP3 batch/stream audit 환경 변수도 모든 PIU10 target에서 사용할 수 있습니다.

### 검증 결과

- Win32 x86 Debug `repiu_aot_probe`와 `repiu` target 빌드가 성공했습니다. 기존 C4819
  code-page와 LNK4217 경고 외에 새 오류는 없습니다.
- `repiu_aot_probe --piu10`의 모든 기존 항목이 통과했습니다.
- frame batch 결과는
  `bytes=4,ecx=103,relocated=true,fail-closed=true`입니다.
- synthetic code/data는 기존 production offset과 다른 위치를 사용하며, cursor alias를
  훼손한 negative arm은 plan을 거부했습니다.
- `pumpito` 정적 disassembly의 실제 feeder loop가 새 matcher 계약과 일치함을 확인했습니다.
- 자동 gameplay 실행은 기존 `repiu_log.txt`를 보존하기 위해 수행하지 않았습니다.

### 남은 검증

사용자 환경에서 `pumpito`의 `verified frame-tail batch active`와 음악·화면 동시 진행을
재확인해야 합니다. 다른 PIU10 target은 자산이 준비된 순서대로 batch 활성 여부를 확인합니다.

### `pumpite` 후속 교정

- 사용자 재실행에서 `pumpitc`는 전체 MP3 입력의 약 98.6%를 batch 처리했지만, `pumpite`는
  `frame-tail batch rejected: shape`와 `batched=0`을 기록했습니다.
- `pumpite` object 2 `+0x1D167`의 정적 명령열은 기존 계약과 의미가 같고 임시 register와
  독립 명령 순서만 달랐습니다. target 이름이나 주소 예외는 추가하지 않았습니다.
- matcher를 제한된 instruction decoder로 바꾸어 cursor/count load-increment-store의 alias와
  def-use, port-register restore 전 clobber 순서를 검증합니다.
- 기존 schedule과 `pumpite` schedule의 synthetic plan이 모두 통과했고 probe는
  `variant=true,fail-closed=true`를 기록했습니다.
- Win32 x86 Debug `repiu_aot_probe` 및 `repiu` 빌드가 성공했습니다. 기존 C4819 경고 외에
  새 오류는 없습니다.
- 사용자 재검증 로그에서 `verified frame-tail batch active`와 checkpoint
  `received/decoded/batched=66478/150/65596`을 확인했습니다. 약 98.7%가 batch 처리되었고,
  실행과 입력 처리가 313초 이상 계속됐으며 사용자가 상태 개선을 확인했습니다. 정상 종료
  통계가 없어 전체 실행 최종 비율은 측정하지 않았습니다.

## English

### Implementation result

- Removed the `pumpito` ROM-set gate, fixed EIP, LE object 4, and `0x3434xx` offsets.
- Derive every state address and service boundary from the feeder shape around the current
  `OUT DX,AL`.
- Validate cursor/count operand aliases, branch targets, guest ranges, and frame state fail closed.
- Attempt batching for every PIU10-capable target while retaining the scalar byte path on mismatch.
- Make MP3 batch and stream audit environment variables available to every PIU10 target.

### Verification result

- Win32 x86 Debug `repiu_aot_probe` and `repiu` targets built successfully, with only existing
  C4819 code-page and LNK4217 warnings.
- Every existing `repiu_aot_probe --piu10` check passed.
- The frame-batch result is `bytes=4,ecx=103,relocated=true,fail-closed=true`.
- Synthetic code and data use locations unrelated to the former production offsets, and a negative
  arm with a mismatched cursor alias rejects the plan.
- Static `pumpito` disassembly matches the new feeder contract.
- Automated gameplay was not run, preserving the existing `repiu_log.txt`.

### Remaining validation

Confirm `verified frame-tail batch active` and concurrent music/gameplay in a live `pumpito` run.
Check batch activation for other PIU10 targets as their assets become available.

### `pumpite` follow-up correction

- The user's `pumpitc` run batched about 98.6% of all MP3 input, while `pumpite` logged
  `frame-tail batch rejected: shape` and `batched=0`.
- Static instructions at `pumpite` object 2 `+0x1D167` implement the same contract with different
  temporary registers and independent instruction ordering. No target-name or address exception
  was added.
- The matcher is now a restricted instruction decoder that validates cursor/count
  load-increment-store aliases and def-use plus clobber ordering before port-register restoration.
- Synthetic plans for both the original and `pumpite` schedules pass, and the probe reports
  `variant=true,fail-closed=true`.
- Win32 x86 Debug builds of `repiu_aot_probe` and `repiu` succeeded with no new errors beyond the
  existing C4819 warnings.
- The user-validation log records `verified frame-tail batch active` and checkpoint
  `received/decoded/batched=66478/150/65596`, or about 98.7% batched. Execution and input handling
  continue for more than 313 seconds, and the user confirms improved behavior. The capture lacks
  orderly shutdown statistics, so no final full-run ratio was measured.
