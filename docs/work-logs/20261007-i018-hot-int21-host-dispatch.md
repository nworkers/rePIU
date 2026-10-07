# 작업 로그: 뜨거운 INT 21h 서비스의 호스트 직접 디스패치 (issue #18)

작업 지시: `docs/work-orders/20261007-i018-hot-int21-host-dispatch.md`
설계: `docs/design/20261007-i018-hot-int21-host-dispatch.md`

## 한 일

1. **두 번째 정지의 원인 확정**: 47~50초 부근 2.5~3.8초 정지 동안
   게스트는 멈추지 않고 HLE 디스패치가 초당 857 → 12,500~19,000회로
   폭증하며 `last_eip`가 lseek 래퍼(AH=42h)였다 — 데모 데이터 파일
   I/O(~45,000호출)가 VEH 왕복으로 늘어진 것.
2. `REPIU_AOT_DBT_SUPERBLOCK` 호스트 디스패치가 이미 발화하지만
   `RequiresVehMediatedHle`가 모든 INT를 일괄 제외해 90초에
   222,309건이 `veh-required`로 폴백함을 확인했다(화이트리스트 없는
   토글 on은 이중 디스패치 오버헤드로 두 번째 정지를 3.1~3.2→
   4.5~4.7초로 오히려 악화).
3. `RequiresVehMediatedHle`에 EAX 매개변수를 더하고 `INT 0x21` +
   AH ∈ {0x2C, 0x3F, 0x42}를 허용 예외로 넣었다(호출부가 frame의
   EAX를 전달). 그 외 INT·far 전송·세그먼트/ESP 기록은 기존대로.

## 측정 (pumpitea 90초, 토글 on/off 교대 2쌍)

| 런 | 1차 공백 | 2차 정지 | dispatch 성공/veh-required |
|---|---|---|---|
| on1 | **2.68초** | **1.37초** | 900,844 / 9,616 |
| off1 | 6.24초 | 4.38초 | 210,214(포트 I/O만) / 0 |
| on2 | **2.53초** | **2.14초** | 767,550 / 9,680 |
| off2 | 4.26초 | 6.53초 | 206,144 / 0 |

두 공백 모두 대략 절반으로 줄고(1차 4.3~6.2→2.5~2.7초, 2차 4.4~6.5→
1.4~2.1초), `veh-required`는 26.6만→9.6천으로 붕괴한다. 예외 0.
1차 공백 2.5~2.7초는 이슈 접수 시점(29초)의 1/11이다.

주의: 최초 측정 한 바퀴는 링크 누락으로 옛 exe를 쟀다(빌드 스크립트의
다중 타깃 호출이 lib까지만 갱신). 산출물 타임스탬프 확인으로 잡았고,
재링크 뒤 재측정했다.

## 검증

* `repiu_aot_probe --selector-guard` 통과.
* 토글 on 30초 스모크: pumpit1·pumpit2a·pumpit3a 모두 예외 0, 프레임
  정상.
* 기능은 기존대로 `REPIU_AOT_DBT_SUPERBLOCK` 옵트인 뒤에 있다.
  **기본 켜기(승격)**는 Task 386(Port-I/O) 선례대로 별도 판단으로
  남긴다 — pumpitea 중심의 증거이므로 더 넓은 게임 확인 후가 적절하다.

---

# Work log: host-direct dispatch for hot INT 21h services (issue #18)

Settled the second stall's cause (demo-data file I/O — HLE dispatches
bursting to 12.5k–19k/s at the AH=42h lseek wrapper, stretched by VEH
round trips), confirmed the existing `REPIU_AOT_DBT_SUPERBLOCK` host
dispatch blanket-rejects every INT (222,309 `veh-required` per 90 s;
the toggle alone even worsens the stall through double dispatch), and
added an INT 21h allowlist (AH 0x2C/0x3F/0x42) to
`RequiresVehMediatedHle` with the caller passing the frame's EAX.

Interleaved 90 s runs: first gap 4.26–6.24 s → **2.53–2.68 s**, second
stall 4.38–6.53 s → **1.37–2.14 s**, dispatch successes 767k–900k,
`veh-required` collapsing to ~9.6k, zero exceptions; the first gap is
now one-eleventh of the issue's original 29 s. One measurement round
was voided by a missed relink (multi-target script run stopped at the
libs; caught by output timestamps). The probe passes and all three
smoke games run clean with the toggle on. The feature stays behind the
opt-in; default-on promotion is left as a separate decision following
the Port-I/O precedent, pending broader game coverage.
