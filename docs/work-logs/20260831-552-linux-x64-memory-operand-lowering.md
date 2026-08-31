# 20260831-552 Linux x64 memory operand lowering 작업 로그

## 한국어

### 결과

Task 546 결정 4를 "guest memory는 하위 4 GiB에 둔다"로 확정하고, 그 확정이 여는 것을
판정기에 반영했습니다. 설계 문서의 결정 4 항목에 확정 사실과 근거를 함께 적었습니다.

memory operand는 이제 `kUnsupported`가 아니라 `kNeedsReencode`이고, **어떤
lowering인지 이름이 붙습니다.**

| 형태 | lowering | 바이트 |
|---|---|---|
| base/index 사용 | `kAddressSizePrefix` | `0x67` 한 바이트를 앞에 |
| `mod=00, rm=101` | `kAbsoluteToSib` | `0x67` + ModRM을 SIB 절대형으로 |
| segment override | 없음 — 계속 `kUnsupported` | — |

`kIdenticalBytes`가 되지는 않습니다. `0x67`을 붙이는 순간 바이트가 달라지므로 복사가
아니라 lowering입니다.

### 왜 절대형은 prefix로 부족한가

`mod=00, rm=101`은 long mode에서 RIP-relative이고, `0x67`은 그것을 끄지 못합니다 —
계산 결과를 32비트로 자를 뿐이라 **EIP-relative**가 됩니다. 여전히 상대 주소입니다.
절대 주소는 SIB(`mod=00`, `rm=100`, SIB `base=101`, `index=100`)로만 표현됩니다.

`0x67`은 SIB 절대형에도 필요합니다. 붙이지 않으면 `disp32`가 64비트로 **sign-extend**
되어 bit 31이 선 주소가 `0xFFFFFFFF8…`이 됩니다. 지금 arena는 `0x085E7000` 아래라
그럴 일이 없지만 그건 우연한 안전이지 규칙이 아닙니다.

### 매뉴얼이 아니라 실행으로 확인했습니다

이 단위의 주장은 "이렇게 바꾸면 같은 주소를 읽는다"입니다. 그건 인용이 아니라 실행이
확인할 것이라, x64 전용 probe가 lowering된 바이트를 실행 가능한 페이지에 써 넣고
실제로 호출합니다.

```text
long_mode_lowering_classification=true
long_mode_lowering_data_page=true
long_mode_lowering_prefix=true,observed=0x5a17c0de
long_mode_lowering_absolute=true,distinct_pages=1,observed=0x5a17c0de,0x5a17c0de
long_mode_lowering_all=true
```

- **prefix**: base register의 상위 절반에 `0xDEADBEEF`를 채워 넣고 호출했습니다.
  `0x67`이 그 절반을 버리지 않았다면 매핑되지 않은 주소를 건드렸을 것입니다. marker가
  나온다는 것이 prefix가 guest의 32비트 산술을 그대로 수행했다는 뜻입니다.
- **absolute**: 같은 lowering된 바이트를 **서로 다른 주소의 두 페이지**에서 실행해
  같은 값을 얻었습니다(`distinct_pages=1`). RIP-relative였다면 두 결과가 갈렸을
  것이므로, 이 한 줄이 재작성이 필요한 이유이자 재작성이 통했다는 증거입니다.

재작성하지 않은 형태는 일부러 실행하지 않았습니다. 그 형태의 문제가 바로 "어디에
놓였느냐에 따라 다른 것을 읽는다"이고, 이 host에서는 매핑되지 않은 주소를 뜻하므로
probe 안에서 증명하면 실행 자체가 끝납니다.

### 검증

| Host | 결과 |
|---|---|
| Linux x64 Debug | `core_probe_all=true`, 17/17, skipped 2 |
| Linux i386 Release | `core_probe_all=true`, 17/17 |
| Win32 x86 Debug | `core_probe_all=true`, 17/17 |

두 32비트 host에서는 `long_mode_lowering` probe가 빌드되지 않습니다. lowering이
불필요한 것을 넘어 **보여줄 것이 없기 때문**입니다 — 그곳에서는 바이트가 이미
재작성이 만들어 내는 의미를 갖습니다. 판정기 자체는 host와 무관하므로 세 host 모두에서
`long_mode_compatibility_all=true`이고, i386과 x64의 `long_mode_*` 16줄은 `diff`로
동일합니다.

## English

### Result

Task 546's decision 4 is settled as "guest memory is placed below 4 GiB", and the
classifier now says what that unlocks. The decision and its evidence are recorded on the
decision itself in the design document.

A memory operand is now `kNeedsReencode` rather than `kUnsupported`, and **the lowering
has a name**.

| Form | Lowering | Bytes |
|---|---|---|
| base/index used | `kAddressSizePrefix` | one `0x67` in front |
| `mod=00, rm=101` | `kAbsoluteToSib` | `0x67` plus a ModRM rewrite into SIB absolute |
| segment override | none -- still `kUnsupported` | -- |

It does not become `kIdenticalBytes`: adding `0x67` changes the bytes, so it is a
lowering rather than a copy.

### Why a prefix is not enough for the absolute form

`mod=00, rm=101` is RIP-relative in long mode and `0x67` does not switch that off -- it
only truncates the computed address, leaving it **EIP-relative**. Still relative. An
absolute address is expressible only through SIB (`mod=00`, `rm=100`, SIB `base=101`,
`index=100`).

The `0x67` is needed on the SIB form too: without it the `disp32` is **sign-extended** to
64 bits, so any address with bit 31 set becomes `0xFFFFFFFF8…`. Today's arena ends below
`0x085E7000` and never sets that bit, but that is accidental safety rather than a rule.

### Confirmed by running it, not by citing the manual

The claim is "rewritten this way, it reads the same address" -- a claim about a
processor. So an x64-only probe writes the lowered bytes into an executable page and
calls them.

- **prefix**: the base register was passed with `0xDEADBEEF` in its upper half. Had
  `0x67` not discarded that half, the instruction would have touched an address nothing
  has mapped. Getting the marker back is the prefix performing the guest's own 32-bit
  arithmetic.
- **absolute**: the same lowered bytes were executed from **two pages at two different
  addresses** and returned the same value (`distinct_pages=1`). RIP-relative bytes would
  have disagreed, so that one line is both the reason the rewrite is needed and the
  evidence that it works.

The un-lowered form is deliberately not executed. Its problem is precisely that what it
reads depends on where it sits, which on this host means an unmapped address --
demonstrating it inside a probe would end the run.

### Verification

| Host | Result |
|---|---|
| Linux x64 Debug | `core_probe_all=true`, 17 of 17, 2 skipped |
| Linux i386 Release | `core_probe_all=true`, 17 of 17 |
| Win32 x86 Debug | `core_probe_all=true`, 17 of 17 |

The `long_mode_lowering` probe is not built on either 32-bit host, because there the
lowering is not merely unnecessary -- it has **nothing to demonstrate**: the bytes already
mean what the rewrite makes them mean. The classifier itself is host-independent, so all
three report `long_mode_compatibility_all=true`, and the sixteen `long_mode_*` lines from
i386 and x64 are identical under `diff`.
