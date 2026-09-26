# Task 742: long mode에서 `mov r,sreg`와 `push sreg`를 trap 없이

## 한국어

### 배경

Task 741의 breakpoint 지점 census에서 pumpitea attract 27초의 breakpoint 102만 회 중 10%가
한 memcpy helper의 두 명령이었다.

```
010FE375  06        push es
010FE376  8C D8     mov eax,ds
010FE378  8E C0     mov es,eax
```

호출마다 `push es`와 `mov eax,ds`가 각각 trap을 낸다(27초에 53,914회씩, 초당 4,000회). 세 번째
`mov es,eax`는 Task 569의 long-mode guarded segment load가 trap 없이 처리한다.

Linux x64의 AOT cache는 long mode에서 돈다. `push es`(`06`)는 long mode에서 무효한 opcode라
planner가 HLE boundary(INT3)로 두고, `mov eax,ds`는 planner가 `kGuardedSegmentRead`로 분류하지만
long-mode 방출기가 없어 INT3로 닫힌다(i386 슬롯은 host segment register를 읽고 shadow와 비교하는
형식이라 long mode에서는 쓸 수 없다). 두 명령 모두 값은 guest 눈에 보이는 selector이고, 그 값은
엔진이 `shadow_selectors->selectors[seg]`(16비트)에 유지한다: guest가 segment를 바꿀 때마다
갱신되고(`instruction_emulation.cpp`, `aot_runtime_dispatch.cpp`), guarded load는 shadow와 같은
값만 통과시킨다. 그러므로 long mode에서 두 명령은 **shadow를 읽는 것**으로 대체할 수 있고 guard가
필요 없다.

### 설계

1. **`mov r16/r32, sreg`** (`kGuardedSegmentRead`, `options.enable_guarded_segment_read`일 때):
   long-mode 슬롯 14바이트.

   | offset | bytes | 뜻 |
   |---|---|---|
   | 0 | `67 0F B7 /r 25 disp32` 또는 `67 66 8B /r 25 disp32 90` | `movzx r32, word [shadow]` / `mov r16, word [shadow]`(+NOP 패드), disp32는 +5 |
   | 9 (16비트는 10) | `E9 rel32` | fallthrough |
   | 14 (16비트는 15) | `CC` | fallback(실행되지 않음, site 계약용) |

   site는 기존 `AotGuardedSegmentReadSite`(`guarded_segment_read_sites`)를 그대로 쓰고
   `shadow_address_offset`과 `load_shadow_address_offset`을 같은 슬롯(+5)으로 둔다. 기존
   `PatchAotGuardedSegmentReadSites`가 그대로 shadow 주소를 써 넣는다. 32비트 형식은 상위 16비트를
   0으로 만든다(실 CPU의 `mov r32,sreg`와 같다).

2. **`push sreg`** (새 kind `kGuardedSegmentPush`; `06`/`1E`/`0F A0`/`0F A8`, operand 32비트일 때만):
   long-mode 슬롯 23바이트.

   | offset | bytes | 뜻 |
   |---|---|---|
   | 0 | `45 8D 7F FC` | `lea r15d,[r15-4]` (guest ESP) |
   | 4 | `67 44 0F B7 34 25 disp32` | `movzx r14d, word [shadow]` |
   | 14 | `45 89 37` | `mov [r15], r14d` |
   | 17 | `E9 rel32` | fallthrough |
   | 22 | `CC` | fallback |

   i386 이미지에서는 원본 바이트를 그대로 복사한다(host가 guest segment를 직접 들고 있으므로).
   site는 read site 목록을 재사용한다(`gpr_register = 0xFF`가 push 표시).

3. `ValidateAotCodeCacheHleCoverage`가 두 슬롯의 배치를 검사한다. long-mode 이미지에서 read 슬롯이
   거부되면 지금처럼 INT3로 닫힌다(i386 read 슬롯은 long mode에 방출하지 않는다).
4. core probe `long_mode_emission`에 세 case를 더한다: `mov eax,ds`, `mov bx,ds`, `push es`의
   슬롯 바이트·site·coverage, 그리고 fallback 바이트 손상 거부.

### 검증 전략

Task 741과 같은 attract 27초에서 `0x010FE375`/`0x010FE376`이 census에서 사라지고 breakpoint 총수가
약 10만 줄어드는지, core probe(Linux x64·Win32)가 통과하는지, pumpitea 플레이와 pumpit2a가
그대로인지 본다.

## English

### Background

Task 741's breakpoint site census put 10% of pumpitea's 1.02 million breakpoints in a 27 s attract on
two instructions of one memcpy helper: `push es` at `0x010FE375` and `mov eax,ds` at `0x010FE376`
(53,914 each, 4,000 a second). The third instruction, `mov es,eax`, is handled without a trap by
Task 569's long-mode guarded segment load.

The Linux x64 AOT cache runs in long mode. `push es` (`06`) is an invalid opcode there, so the planner
leaves it an HLE boundary (INT3); `mov eax,ds` is classified `kGuardedSegmentRead` but has no
long-mode emitter and is closed with INT3 (the i386 slot reads the host segment register and compares
it with the shadow, which cannot work in long mode). Both instructions produce the guest-visible
selector, which the engine keeps in `shadow_selectors->selectors[seg]` (16-bit): updated on every
guest segment change and enforced by the guarded load. In long mode both can therefore become **a read
of the shadow**, with no guard.

### Design

1. **`mov r16/r32, sreg`** (`kGuardedSegmentRead`, under `options.enable_guarded_segment_read`): a
   15-byte long-mode slot (16 with the 16-bit form's NOP pad): `67 0F B7 /r 25 disp32` (`movzx r32,
   word [shadow]`) or `67 66 8B /r 25 disp32 90` (`mov r16, word [shadow]`), the displacement at +5,
   then `E9 rel32` to the fallthrough at +9 (+10) and a `CC` fallback at +14 (+15) that never runs.
   The site is the existing `AotGuardedSegmentReadSite` with both address offsets at +5, so
   `PatchAotGuardedSegmentReadSites` fills it unchanged. The 32-bit form zeroes the
   upper 16 bits, as a real CPU does.
2. **`push sreg`** (new kind `kGuardedSegmentPush`; `06`/`1E`/`0F A0`/`0F A8`, 32-bit operand only): a
   23-byte long-mode slot: `45 8D 7F FC` (`lea r15d,[r15-4]`, the guest ESP), `67 44 0F B7 34 25
   disp32` (`movzx r14d, word [shadow]`), `45 89 37` (`mov [r15], r14d`), `E9 rel32` at +17 and `CC`
   at +22. On i386 images the original bytes are copied (the host holds the guest segments itself).
   The site reuses the read-site list with `gpr_register = 0xFF` marking a push.
3. `ValidateAotCodeCacheHleCoverage` checks both layouts. A refused read in a long-mode image closes
   with INT3 as today; the i386 read slot is never emitted into a long-mode image.
4. Three core-probe cases in `long_mode_emission`: `mov eax,ds`, `mov bx,ds` and `push es` (slot
   bytes, site, coverage) plus rejection of a corrupted fallback byte.

### Verification strategy

Task 741's 27 s attract: `0x010FE375`/`0x010FE376` gone from the census and about 100,000 fewer
breakpoints; core probes (Linux x64 and Win32) pass; pumpitea play and pumpit2a unchanged.

---

## 결과 (구현 후) / Result (after implementation)

### 한국어

설계대로 두 슬롯을 넣었습니다. 두 지점은 census에서 사라졌고(30초 0회), breakpoint 총수는 878,285회
(Task 741의 1.02M~1.22M), VEH 벽시계 비중 6.65%입니다. 설계와 달라진 점: read 슬롯의 disp32는 +4가
아니라 +5(`67` + 2바이트 opcode + modrm + SIB)이고, 그래서 슬롯은 15/16바이트, fallback은 +14/+15입니다.
또 기존 코드가 long-mode 이미지에 i386 read 슬롯을 방출하고 있었음을 확인해(host DS와 RIP 상대 비교로
항상 fallback) generic switch에서 막았습니다. 검증기 오프셋 실수가 미번역 간접 call 대상에서 단일
스텝이 거절되는 잠복 결함을 드러냈습니다(작업 로그 4항).

### English

Both slots went in as designed. The two sites are gone from the census (zero in 30 s), breakpoints are
878,285 (1.02–1.22 million in Task 741) and the VEH's share of wall is 6.65%. Departures from the
design: the read slot's displacement is at +5, not +4 (`67` plus a two-byte opcode, modrm and SIB), so
the slot is 15/16 bytes with the fallback at +14/+15; and the existing code was found to emit the i386
read slot into long-mode images (always falling back after comparing the host DS RIP-relatively), which
the generic switch now prevents. A validator offset mistake exposed a latent defect: a single step
refused at an untranslated indirect-call target (work log, step 4).
