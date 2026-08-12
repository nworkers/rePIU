# Low memory string instruction 대행 작업 로그

## 결과

* `SCAS`, `LODS`, `CMPS`의 DOS low memory 읽기를 대행하는
  `low_memory_string_access` 모듈을 추가했습니다.
* `pumpit8`의 iCCP 접근 위반 종료가 사라졌고, 실행이 해당 지점을 통과합니다.

## 구현 내용

1. `SetCompareFlags8`을 폭 인자를 받는 `SetCompareFlags`로 일반화하고 기존 함수는 이를
   호출하도록 바꿨습니다. `CF`, `PF`, `AF`, `ZF`, `SF`, `OF`를 8/16/32-bit 폭에서 계산하며
   8-bit 결과는 이전과 같습니다. `repne`의 종료 판정이 `ZF`에 의존하므로 필요한 일반화입니다.
2. `ServiceGuestLowMemoryStringInstruction`은 `REP` 계열이 반복 사이에서 재개 가능하다는
   성질을 이용합니다. 반복이 끝났을 때만 `EIP`를 전진시키고, 남았으면 `EIP`를 그대로 두어
   CPU가 현재 `ECX`/`ESI`/`EDI`에서 같은 명령을 이어서 실행하게 합니다. 덕분에 handler가
   low memory 경계를 추적할 필요가 없습니다.
3. 폭은 operand width가 아니라 mnemonic에서 얻습니다. `66h` prefix는 mnemonic 자체를
   바꾸므로(`SCASW` 대 `SCASD`) 이 매핑이 정확합니다.
4. `ZYDIS_CATEGORY_STRINGOP`으로 먼저 거릅니다. SSE `cmpsd`가 string 형과 mnemonic을
   공유하기 때문입니다.
5. 거부 경계는 다음과 같습니다. address size가 32-bit가 아닌 경우, 접근이 low memory 경계를
   걸치는 경우, `CMPS` 반대편이 guest 범위 검사를 통과하지 못하는 경우, `REP`인데 `ECX`가
   이미 0인 경우입니다. 모두 대행하지 않고 기존 진단이 원인을 그대로 보고하게 둡니다.
6. `MOVS`는 `HandleRepMovsInstruction`이 이미 전담하므로 제외했고, `STOS`는 쓰기이므로
   이 읽기 대행의 범위에서 제외했습니다.

## 검증

* `cmake --build build/win32_x86_debug --config Debug --target repiu`: 성공
* Debug `pumpit8` 실행 결과

  | 항목 | 값 |
  |---|---|
  | string 대행 횟수 / 반복 수 | 2,522 / 2,522 |
  | 마지막 EIP / 주소 | `0x040E5D0D` / `0x00000017` |
  | 마지막 mnemonic / 회당 반복 | `SCASB` / 1 |
  | 예외 발생 | 0건 |
  | 도달 지점 | `AUDIO/004.AUD` |

* 수정 전에는 약 97초에 `0xC0000005`로 종료했습니다. 수정 후에는 148초 이상 예외 없이
  실행하며 SDL 창이 약 6.9 FPS로 렌더링했고, 관측을 끝내기 위해 SDL 종료 요청으로
  정상 종료시켰습니다.
* 회당 반복 수가 1인 것은 low memory image가 0으로 초기화되어 `repne scasb`가 첫 바이트에서
  종료되기 때문이며 설계 예측과 일치합니다.
* `MOV` 경로 계수는 수정 전후 모두 0으로 기존 경로 동작은 바뀌지 않았습니다.

## 남은 것

이 지점만 닫혔습니다. 이후 구간의 진행 여부는 별도 관측 대상입니다.

# Low Memory String Instruction Servicing Work Log

## Result

* Added the `low_memory_string_access` module servicing `SCAS`, `LODS`, and `CMPS` reads of DOS low
  memory.
* The `pumpit8` iCCP access-violation termination is gone and execution passes that site.

## Implementation

1. Generalized `SetCompareFlags8` into a width-taking `SetCompareFlags`, with the original
   delegating to it. It computes `CF`, `PF`, `AF`, `ZF`, `SF`, and `OF` at 8/16/32-bit widths and
   leaves 8-bit results unchanged. The generalization is required because `repne` termination
   depends on `ZF`.
2. `ServiceGuestLowMemoryStringInstruction` exploits the restartability of `REP` forms between
   iterations: `EIP` advances only when the repetition finished, and is otherwise left in place so
   the CPU resumes the same instruction from the current `ECX`/`ESI`/`EDI`. The handler therefore
   never tracks where low memory ends.
3. Width comes from the mnemonic rather than the operand width, since a `66h` prefix changes the
   mnemonic itself (`SCASW` versus `SCASD`), making the mapping exact.
4. `ZYDIS_CATEGORY_STRINGOP` gates the path first, because the SSE `cmpsd` shares its mnemonic with
   the string form.
5. The declining boundaries are a non-32-bit address size, an access straddling the low-memory
   boundary, a `CMPS` counterpart the guest-range check rejects, and a `REP` whose `ECX` is already
   zero. Each declines so the existing diagnostics report the real cause.
6. `MOVS` is excluded because `HandleRepMovsInstruction` already owns it, and `STOS` is excluded
   because it writes and this facility services reads.

## Verification

* `cmake --build build/win32_x86_debug --config Debug --target repiu`: passed
* Debug `pumpit8` run

  | Item | Value |
  |---|---|
  | String services / iterations | 2,522 / 2,522 |
  | Last EIP / address | `0x040E5D0D` / `0x00000017` |
  | Last mnemonic / iterations per service | `SCASB` / 1 |
  | Exceptions raised | 0 |
  | Reached | `AUDIO/004.AUD` |

* Before the change the run terminated with `0xC0000005` at roughly 97 seconds. After it, execution
  ran past 148 seconds without an exception while the SDL window rendered at roughly 6.9 FPS, and
  it was ended with an SDL exit request to stop the observation.
* One iteration per service matches the design prediction: the low-memory image is zero-initialized
  so the `repne scasb` terminates on its first byte.
* The `MOV` path counter is zero both before and after the change, so that path is unaltered.

## Remaining

Only this site is closed. Whether later stages proceed is a separate observation target.
