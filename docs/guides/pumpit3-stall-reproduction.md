# pumpit3 멈춤 재현·판정 가이드 / Reproducing and Classifying the pumpit3 Stall

증상과 확인된 사실은 [pumpit3 기동 중 멈춤](../analysis/pumpit3-startup-stall.md)에
있습니다. **EIP census 자체의 절차와 한계는
[실행 정지 지점 EIP census 가이드](execution-stall-eip-census.md)를 그대로 씁니다** —
이 문서는 그 위에 pumpit3 멈춤에만 해당하는 것을 더합니다.

## 0. 이 멈춤은 해소됐습니다 (Tasks 414 · 417, 2026-08-04)

기본 빌드에서는 **더 이상 재현되지 않습니다**(60초 8회 중 8회 정상). 따라서 이 문서는
이제 **회귀 확인과 A/B 재현** 절차입니다. 원인 둘은 각각 스위치로 되살릴 수 있습니다.

| 되살릴 원인 | 스위치 | 되돌아오는 증상 |
|---|---|---|
| 포화 (Task 414) | `REPIU_PORT_IO_DELAY_LOOP=0` | 14회 중 0회 정상, 프레임 0~1 |
| arena 낙하 (Task 417) | `REPIU_AOT_STRICT_SPANNING_ENTRY=1` | 5회 중 2회 멈춤, single-step 1.6~1.7M |

두 스위치를 끄고(기본값) 돌린 실행이 멈추면 **새 원인**이므로 §2·§3 절차로 좁힙니다.

## 1. 재현

기본 빌드는 정상이므로 재현하려면 §0의 스위치 중 하나를 켭니다. 원래 재현율 **약
29%**(17회 중 5회)는 두 원인이 모두 살아 있던 빌드의 값이므로, 어느 쪽이든 한 번에
여러 회 돌립니다. EEPROM은 **실행별로 격리**합니다(공유하면 영속 상태가 새어 결과가
무효가 됩니다).

```
set REPIU_EXECUTION_BACKEND=aot-dbt
set REPIU_EXECUTION_TIMEOUT_MS=60000
set REPIU_EEPROM_PATH=<실행별 사본>
build\win32_x86_debug\Release\repiu_loader_win32.exe pumpit3 > run.txt 2>&1
```

**PowerShell 리다이렉션을 쓰지 마십시오.** 콘솔 폭 120자에서 줄이 잘려 census 값이
중간에서 끊깁니다. `cmd /c "... > run.txt 2>&1"` 형태로 받습니다.

45초면 판정에 충분합니다. 240초까지 늘려도 멈춘 실행은 진행하지 않으므로, **오래
돌리는 것으로는 아무것도 더 얻지 못합니다**(분석 문서 확인 1).

## 2. 판정 — 한 줄로 갈립니다

```
grep -c "DOS path trace #" run.txt
```

| 값 | 판정 |
|---:|---|
| **6** | **멈춤.** 마지막 파일이 `stage.cfg`입니다 |
| 8 이상 | 정상. 45~60초 기준 프레임 1,300~1,900 |

교차 확인용 서명(멈춘 실행 5회가 전부 일치):

```
Win32 Glide call trace: ... _GRBUFFERSWAP@4 count=   → 0 또는 1
Win32 AOT generation publishes/quarantines:          → 79/1
Win32 DOS AH hotspots [...]                          → 11/12 항목이 없음
```

`AH=11h`/`12h`는 정상 실행이 **프레임마다 1회씩** 부르므로 그 수가 곧 프레임 수입니다.
**이 항목이 아예 없으면 프레임 루프에 한 번도 진입하지 못한 것입니다.**

**판정에 쓰지 말 것:** `publishes/quarantines`의 격리 수. 당시에는 정상 실행도 같은
페이지를 같은 사유로 격리해 갈림이 되지 않았습니다(분석 문서 확인 4). Task 415가
벌칙을 주소로 좁힌 뒤로는 **기본 경로에서 격리 자체가 나오지 않으므로** 위 서명의
`79/1`도 옛 빌드의 값입니다.

**Task 415 이후 함께 읽을 줄** — 세대 실패 정책의 현재 상태입니다.

```
Win32 AOT generation failure addresses/skips/quarantine-fallbacks/spanning-activations: a/b/c/d
```

| 관측 | 판정 |
|---|---|
| `a` = 0, `d` ≥ 1 | 정상. 걸친 요청 항목이 활성화돼 실패가 아예 없습니다 |
| `a` ≥ 1 | 세대 실패가 남아 있습니다. `b`(건너뛴 시도)가 크면 그 주소를 게스트가 계속 밟는 중입니다 |
| `c` ≥ 1 | 페이지 단위 격리로 되돌아갔습니다 — 스위치가 켜졌거나 억제 집합이 찼습니다 |

## 3. 두 덤프를 뺄 때의 비대칭 (이 증상에서 특히 중요)

핫스팟 census가 single-step 경계만 담는다는 한계는
[EIP census 가이드 §5](execution-stall-eip-census.md)에 있습니다. 멈춤 조사에서는
그 한계가 **방향에 따라 다르게 작용**하므로 별도로 적어 둡니다.

* **"멈춤에만 있는 주소"로 결론 내지 마십시오.** 이번 분석에서 282개가 나왔는데 전부
  격리 페이지였고, 이는 "정상 실행이 그 코드를 안 돈다"가 아니라 **"정상 실행에서는
  캐시로 돌아 기록되지 않았다"**는 뜻입니다.
* **"정상에만 있는 주소"는 유효합니다.** 정상 실행에서 trace가 켜진 채 지나간 코드를
  멈춘 실행이 한 번도 밟지 않았다는 뜻이기 때문입니다.

## 4. 주소 변환 두 가지를 구분하십시오

| 목적 | 변환 |
|---|---|
| 원본 **파일 offset** (기존 가이드) | `guest = file + 0x02FF4E00` (pumpit3 object 2) |
| **`repiu_aot_probe --dump`** 인자 | `probe = live - 0x02000000` |

프로브는 이미지를 `0x01000000` 기준으로 매핑하고(entry `0x010D00A0`) 실행 시 arena
base는 보통 `0x03000000`이기 때문입니다. 변환을 빼면 `--dump`가 `mapped=false`만
돌려줍니다. 실행 로그의 `Runtime memory arena base`를 먼저 확인하십시오 —
`0x07000000`으로 잡히는 실행도 있고, 그때는 `-0x06000000`입니다.

---

# Reproducing and Classifying the pumpit3 Stall

The symptom and confirmed facts are in
[pumpit3 startup stall](../analysis/pumpit3-startup-stall.md), and the EIP census
procedure and its limits are taken as-is from the
[execution stall EIP census guide](execution-stall-eip-census.md). This page adds only
what is specific to the pumpit3 stall.

## 0. This stall is fixed (Tasks 414 and 417, 2026-08-04)

The default build **no longer reproduces it** (eight healthy runs of eight at 60 seconds),
so this page is now a regression check and an A/B reproduction procedure. Each cause can be
brought back with its switch: `REPIU_PORT_IO_DELAY_LOOP=0` restores the saturation (zero
healthy runs in fourteen, 0-1 frames), and `REPIU_AOT_STRICT_SPANNING_ENTRY=1` restores the
arena fall-through (two stalls in five, 1.6-1.7 M single steps). **A stall with both switches
off is a new cause**, to be narrowed with the sections below.

## 1. Reproduce

To reproduce, enable one of the switches above; the original **29%** rate (five of seventeen)
belongs to the build where both causes were live, so run several either way. Keep
the EEPROM **isolated per run** — sharing it leaks persistent state and invalidates the
result. Use the `aot-dbt` backend with a 60-second timeout. **Do not redirect through
PowerShell**: it truncates lines at the 120-column console width and cuts census values
in half; use `cmd /c "... > run.txt 2>&1"`. Forty-five seconds is enough to classify, and
running longer gains nothing — a stalled run still has not advanced at 240 seconds.

## 2. Classify — one line decides it

`grep -c "DOS path trace #"` returns **6** for a stalled run, whose last file is
`stage.cfg`, against eight or more for a healthy one. Cross-check the signature, identical
across all five stalled runs: `_GRBUFFERSWAP@4` count of zero or one, AOT
`publishes/quarantines` of `79/1`, and **no `11`/`12` entry** in the DOS AH hotspots.
Healthy runs issue `AH=11h`/`12h` once per frame, so that count is the frame count and its
absence means the frame loop was never entered.

**Do not classify on the quarantine count**: healthy runs quarantined the same page for
the same reason, so it never separated the two — and since Task 415 narrowed the penalty to
an address, the default path produces **no quarantine at all**, which also makes that `79/1`
signature a figure from the old build. Read the policy line instead:
`Win32 AOT generation failure addresses/skips/quarantine-fallbacks/spanning-activations`.
Zero failed addresses with one or more spanning activations is healthy; a nonzero address
count means a generation failure survives, with the skip count showing how often the guest
still reaches it; a nonzero quarantine fallback means the page-wide penalty is back, either
by switch or because the suppression set filled.

## 3. Subtracting two dumps is asymmetric

The census-only-samples-single-steps limit is in the EIP census guide; in this
investigation it bites differently depending on direction. **Never conclude from
"addresses only in the stalled run"** — all 282 of them here were the quarantined page,
meaning only that healthy runs execute that code from the cache. **"Addresses only in the
healthy run" is valid**: it names code the stalled run never reached.

## 4. Keep the two address conversions apart

To reach a file offset use `guest = file + 0x02FF4E00` (pumpit3 object 2, per the EIP
census guide). To feed `repiu_aot_probe --dump` use `probe = live - 0x02000000`, since the
probe maps at `0x01000000` (entry `0x010D00A0`) while a run usually places the arena at
`0x03000000`; without it the probe answers `mapped=false`. Check `Runtime memory arena
base` first — some runs land at `0x07000000`, where the offset is `0x06000000`.
