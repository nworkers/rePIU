# pumpitea 로딩 공백과 세그먼트 flip·재패치 결합 분석

issue #18의 로딩 공백(ANDAMIRO 로고 다음 프레임 없는 구간) 회귀를 쫓으며
확인한 사실을 누적한다. 측정은 모두 Win32 Release, `REPIU_STALL_TIMEOUT_MS=0`,
`REPIU_EXECUTION_TIMEOUT_MS=90000`, `REPIU_GLIDE_FRAME_RATE_LOG=1`, 창 보임,
vsync 기본에서 했다(2026-10-06, Intel HD 620 노트북).

## 확인됨

### ES flip의 정체: 게스트 memcpy 관용구

게스트 오프셋 `0xFE378`·`0xFE38A`(이미지 base `0x04000000`)의 ES 교대는
C 런타임 memcpy의 세그먼트 저장·복원이다. `repiu_aot_probe --dump 0x010FE360`
(probe base `0x01000000`)으로 디스어셈블했다.

```
0xFE36A  8B 7C 24 04   mov edi, [esp+4]
0xFE36E  89 D1         mov ecx, edx
0xFE370  8B 75 00      mov esi, [ebp+0]
0xFE373  8B 5C 24 08   mov ebx, [esp+8]
0xFE375  06            push es          ; 네이티브 복사
0xFE376  8C D8         mov ax, ds       ; 가상 DS = 0x0024
0xFE378  8E C0         mov es, ax       ; ES ← 0x0024 (guarded load 지점)
0xFE37A  57 89 C8 C1 E9 02 F2 A5        ; push edi; … ; rep movsd
0xFE382  8A C8 80 E1 03 F2 A4           ; 나머지 바이트 rep movsb
0xFE389  5F            pop edi
0xFE38A  07            pop es           ; ES ← 0x002B 복원 (guarded pop 지점)
```

`segment load trace`로 selector를 확인했다: `0xFE378`은 `0x0024`,
`0xFE38A`는 `0x002B`(source `0x049E0744`, 스택의 저장값). 2026-10-07에
마운트된 실행 파일(`build/runtime_mounts/pumpitea/PIU/PIU.EXE`)을 다시
떠서 `0xFE38A`가 **`pop es`**(guarded pop 슬롯)임을 확정했다 — 이전 기록의
"`8E 06?` mov es, [...]" 추정은 오류였다. flip은 memcpy 호출마다
일어나므로 로딩 중 초당 약 1,100~2,400회다.

attract 구간과 틱 경로에는 같은 모양의 **DS flip**이 이어진다(같은 덤프로
확정):

```
0xFE2F0  66 2E 8E 1D F9 E2 0F 01   mov ds, cs:[0x010FE2F9]  ; INT8 ISR 진입 헬퍼,
                                                            ; 0x66 접두어 + CS 접두어,
                                                            ; 메모리-소스 로드 (슬롯 없음 → HLE boundary)
0xFE2F8  C3                        ret
…
0x2AB7E  FB FC                     sti; cld
0x2AB80  0F A9                     pop gs     ; guarded pop
0x2AB82  0F A1                     pop fs     ; guarded pop
0x2AB84  07                        pop es     ; guarded pop
0x2AB85  1F                        pop ds     ; guarded pop, DS ← 0x002B
0x2AB86  61 CF                     popad; iretd
```

런 간 고정 2,520건의 `0xFCF6F` 그룹은 far 문자열 비교 관용구다:
`8C D9 mov cx,ds; 8C EA mov dx,gs; 89 EE; 1E push ds; 06 push es; 91 xchg
eax,ecx; 8E DA mov ds,dx; 8E C0 mov es,ax (0xFCF6F); 09 C9; F3 A6 repe
cmpsb; …; 07 pop es; 1F pop ds`.

### 두 selector의 해석

`REPIU_AOT_SEGMENT_RESOLUTION_TRACE=1`로 확인한 ES 해석:

| selector | base | limit | 비고 |
|---|---|---|---|
| 0x002B | 0x00000000 | 0x0000FFFF | 호스트 flat, native fold |
| 0x0024 | 0x00000000 | 0x000F76BB | DOS 저메모리 한계 아래 → override 지점은 HLE 라우팅 |

재패치가 지점에 실제로 바꿔 쓰는 것은 가드의 selector 즉치값과
native/HLE 라우팅 형태뿐이다(base는 둘 다 0).

### v0.0.180이 이 경로에 들어오지 않았던 이유

v0.0.180의 Win32는 게스트의 DS를 가상으로 추적하지 않아, memcpy의
`mov ax, ds`가 물리 DS(호스트 flat `0x2B`)를 읽었고 `mov es, ax`는
현재 ES와 같은 값의 no-op 로드가 되어 guarded 지점이 네이티브로
성공했다(180 종료 요약: guarded load 성공 75,905 / 폴백 2,532).
Linux x64 이식(v0.0.181~191)에서 DS가 가상 selector(`0x0024`)로
추적되면서 `mov es, ax`가 실제 값 변경이 되었고 flip이 생겼다.
가상 추적이 실기 DOS/4GW(게임은 자신의 LE data selector로 돈다)에
더 충실하므로, 180의 동작은 우연한 지름길이었다.

### 캐시 전체 재패치는 순수한 낭비가 아니다 (issue 방향 1·2 기각)

issue #18의 방향 1(바뀐 레지스터의 지점만 재패치)과 2(쓰는 페이지만
`ProtectMemory`)를 구현해 측정했다. 같은 날 교대로 잰 90초 실행의
공백(span_ms 최대 간격):

| 빌드 | 공백(초) | handled segment load | guarded load 성공/폴백 |
|---|---|---|---|
| main(v0.0.205) 기준선 ×3 | **18.7 / 15.0 / 19.6** | 42.6k~47.3k | 47.5k~68.4k / 6.0k~7.3k |
| 마스크 v1 (override=해석 변화, guarded=shadow 변화) ×3 | **35.1 / 45.5 / 45.2** | 106k~118k | 12.5k / 40.8k |
| 마스크 v2 (둘 다 해석 변화) ×2 | **52.6 / 60.8** | 73.5k~77.2k | 25.7k~29.9k / 23.4k~25.5k |

기준선 3차(19.6초)는 v2 직후에 측정해 열 스로틀 교란을 배제했다.
마스크 변형은 둘 다 공백을 2~3배 **악화**시켰고, guarded 지점의
네이티브 성공이 무너졌다(기준선 ~90% 성공 → v1 ~23%). 재패치당
지점 기록 수는 1/5~1/12로 줄었는데도(종료 요약 selector guard 합산
210M → 45M) 벽시계 시간이 나빠졌으므로, 전량 재패치는 해석 갱신
이상의 일을 하고 있다.

### 로딩 구간은 예외 디스패치가 지배한다

30초 census(마스크 v1 빌드): 예외 총 296,966건 — single-step 2,281,
**breakpoint 209,722**, access-violation 23,716, 특권 명령(0xC0000096)
61,247. breakpoint 상위는 `0x04101297`(50,498), `0x04101BAE`(44,983),
`0x041012D3`(35,349), 그 다음이 flip 지점 `0xFE38A/0xFE378/0xFE376`
(각 16,741). 기준선 90초의 특권 명령 수(223,392)와 마스크 빌드
(227,277)는 비슷해, 차이는 breakpoint 쪽이다.

### 게스트 페이지 리타이어가 flip 블록 근처를 닫는다

두 빌드 모두 실행 초기에 `retire guard reset #1 page=0x04107000
guards=1 entries=1466`(generation 227) 한 번으로 **주소 맵 엔트리
1,466개가 INT3(0xCC)로 닫힌다**. `RetireAotGuestPage`는 엔트리의
`cache_offset`에 0xCC를 쓰고 비활성으로 표시한다.

### 뜨거운 breakpoint 지점은 DOS 시간·seek 호출이다

30초 breakpoint site census 상위는 리타이어 재진입이 아니라 DOS
호출이다(기준선/마스크 v1 비슷한 횟수):

| 게스트 주소 | 정체 | 기준선 | v1 |
|---|---|---|---|
| `0x04101297` | `calibrate()`의 1초 계수 루프(INT 21h AH=2Ch) | 53,891 | 50,498 |
| `0x041012D3` | `delay(ms)`의 스핀 루프(AH=2Ch) | 37,724 | 35,349 |
| `0x04101BAE` | lseek(AH=42h) | 33,598 | 44,983 |
| `0x04101287` | `calibrate()`의 초 경계 동기 루프 | 7,992 | 7,411 |
| `0x040FE376/378/38A` | memcpy flip 슬롯의 가드 폴백 | **3,139씩** | **16,741씩** |

디스어셈블로 확인: `calibrate()`(게스트 `0x101277`)는 AH=2Ch의 DH(초)가
바뀔 때까지 동기한 뒤 **한 실제 초 동안** 호출 수를 세어
`[0x047E04A8]`에 저장하고, `delay(ms)`(`0x1012A9`)는
`ms×저장값/1000+0.5`회 AH=2Ch를 스핀한다. delay의 호출처 여섯 곳은
초기화의 `delay(5000)` 하나와 **I/O 보드 포트(0x2A4/0x2AC/0x2DA) 쓰기
뒤의 delay(100~500)** 들이다. 즉 로딩 중 보드 설정마다 게임이 스스로
0.1~0.5초를 기다리고, AH=2Ch HLE는 실제 벽시계를 돌려준다
(`HandleDosGetSystemTime`).

### 시간 예산: VEH가 벽시계의 65%, 그 안의 80%는 미계측 잔여

기준선 30초 `REPIU_EXECUTION_TIME_PROFILE=1`: `execution time share
veh/glide-gate/port-io/dos = 65.22%/17.63%/0.17%/0.40%`(+미계상 17.15%).
VEH 283,208회, 평균 ~19만 사이클. VEH 하위 버킷은 prologue 0.59% /
aot-transfer 7.09% / hle-chain 12.35% / **잔여 79.81%**. DOS 서비스
자체(hle-chain)가 아니라 VEH 경로의 버킷 밖 작업이 비용이다.

### 호스트 심볼 귀속: 잔여의 주체는 재패치 경로

guest position census(30초, host 표본 ~87%)의 사이트 상위:

* 기준선: `ProtectMemory+0x5F` 29.5%, `PatchAotGuardedSegmentLoadSites`
  17.6%, `ReResolveWin32AotSegmentOverrides` 8.9%, pop/override 패처
  각 3% → **재패치 경로 합 ~62%**.
* 마스크 v1: `ProtectMemory` 27.3%, `ReResolveWin32…+0x148`(범위 사전
  패스) 15.2%, `PatchAotSegmentOverrideSites` 13.8% → 마스크로도 합
  ~56%. 지점 기록을 1/12로 줄여도 flip 트랩 횟수가 ~5배로 늘어
  재패치 호출 자체가 잦아졌기 때문이다.

### 실행이 두 모드로 갈리고, 기계 상태가 모드를 고른다

같은 바이너리의 90초 실행이 두 모드로 나뉜다. **빠른 모드**: flip
슬롯 트랩이 드물고(30초에 ~3.1k) guarded 성공 우세, 공백 15~19.6초.
**느린 모드**: flip이 전부 슬롯 폴백으로 트랩(30초에 ~16.7k), 공백
32~61초. 오전의 교대 측정에서 기준선은 3회 모두 빠른 모드(15.0/18.7/
19.6초), 마스크 v1은 3회 모두 느린 모드(35.1/45.5/45.2초)였다. 그러나
**약 2시간 연속 부하 뒤에는 수정 없는 기준선도 느린 모드(40.2초)로
측정**되어, 기계 상태(열 스로틀 추정)가 모드 선택에 개입함이
확인됐다. 이 때문에:

* v1의 회귀는 교대 측정으로 뒷받침된다(기준선이 중간에 15.0초).
* v2(52.6/60.8초)는 직후 기준선 19.6초로 범위가 잡힌다.
* **v3**(쓰기 동작은 기준선과 동일, `ProtectMemory` 범위만
  capacity→size)의 32~44초는 동시대 기준선이 40.2초라 **판정 불가**
  (드리프트 교란). 통제된 기계에서 재측정해야 한다.

### flip은 인터럽트 문맥 코드에서 일어난다 (전수 트레이스)

`REPIU_DPMI_SEGMENT_TRACE=1` 20초 전수 트레이스(세그먼트 로드
14,151건)로 확인했다.

* INT8 ISR(게스트 `0x2AAE4`)은 진입부에서 헬퍼 `0x0FE2F0`
  (`mov ds, cs:[0x0FE2F9]`)로 **DS←0x0024**를 올리고, 말미
  `0x2AB85`의 `pop ds`로 중단된 값 **0x002B**를 복원한다. 같은 모양의
  쌍이 `0x101CC4→0x101DC6`, `0xFD81B→0xFD843`, `0xFCE13→0xFCE23`
  (DS=0x0090) 등 여러 호출부에 있다.
* **memcpy ES flip(`0xFE378/0xFE38A`)은 모두 `ds=0x0024` 상태에서,
  `0xFE2F0` 직후~복원 전 구간에서 실행된다**(이 런의 flip 837건 전부
  ds=0x0024). 즉 flip은 메인라인이 아니라 FE2F0 헬퍼로 진입한
  인터럽트 문맥 코드의 memcpy다.
* 메인라인 DS는 0x002B로 유지되고 복원 지점들은 항상 0x002B로
  돌아간다. "메인라인 DS가 0x0024로 흡수되어 모드가 갈린다"는 중간
  가설은 이 트레이스로 **기각**됐다.
* 빈도(20초): `0xFE2F0` 진입 3,331회(~167/s), INT8 말미 `0x2AB85`
  복원 1,306회(~65/s — 틱 주기로 보임), flip 837회(~42/s). 진입이
  복원보다 ~2.5배 많으므로 FE2F0 헬퍼는 INT8 외의 인터럽트·콜백
  진입부에서도 쓰인다.

### flip 트랩의 사전 상태 전수 확인 (확장 트레이스)

`TraceGuestSegmentEvent`에 물리 Seg*와 **갱신 전** shadow 값을 더하고
(`REPIU_DPMI_SEGMENT_TRACE`, 기록 시점을 모든 상태 기록 앞으로 이동),
`cmd /c` 리디렉션으로 줄바꿈 없는 25초 전수 기록을 떴다. ES 로드
트랩의 사전 상태:

| EIP | 로드 값 | 물리 ES | shadow ES | 건수 | 판독 |
|---|---|---|---|---|---|
| 0xFE378 | 0x0024 | 0x002B | 0x002B | 4,421 | 물리≠새 값 → **구조적으로 항상 폴백** |
| 0xFE38A | 0x002B | 0x002B | **0x0024** | 4,420 | 직전 HLE가 남긴 shadow 때문에 항상 폴백 |
| 0xFCF6F 등 | 0x002B | 0x002B | 0x002B | 2,520 | 세 비교 전부 통과 상태인데 트랩 — 슬롯이 네이티브 가드 형태가 아님(닫힘/미번역 경로). 횟수가 런 간 고정(1,860/220×3) |

따라서 현 설계에서 **memcpy 1회 = 최소 폴백 2회 + ES 재해석 2회 +
전량 재패치 2회**가 하한이고, "guarded 성공" 카운터는 flip이 아니라
같은 selector 재로드·pop 등 다른 지점에서 나온다.

### 방향 3의 설계 씨앗 (이 데이터에서 직접 도출)

* 복원 쪽(0xFE38A): `물리==새 값 && shadow만 불일치`인 경우, 슬롯이
  **shadow 워드를 네이티브로 쓰고 통과**하면 폴백·재해석·재패치가
  모두 사라진다. 정확성은 override 가드의 자기검증(접은 selector와
  shadow 비교)이 지킨다. 단 `context->guest_es`가 낡게 되므로 HLE
  경계가 세그먼트 상태를 shadow에서 읽도록 진실원을 옮겨야 한다.
* 로드 쪽(0xFE378): 물리 비교는 Win32에서 게스트 selector에 대해
  영원히 실패하므로(위 `--segment-restore` 확정), 레지스터별 "허용
  대체 selector" 한 칸을 두고 `새 값==대체`면 같은 방식으로 shadow만
  쓰게 하면 flip 전체가 트랩 없이 돈다.

* 느린 모드의 본질은 memcpy flip이 **AOT 캐시 슬롯 경로**(가드 폴백
  INT3→HLE→재해석→재패치)로 도는 것이고, 빠른 모드에서는 같은
  명령이 다른 경로(직접 실행/다른 처리)로 돌아 슬롯 폴백과 재패치가
  거의 생기지 않는 것이다. flip 슬롯 트랩 횟수가 모드와 1:1로
  움직인다. 모드 선택은 실행 초기의 타이밍(동적 번역 완료 시점,
  스레드 스케줄링, 스로틀)에 민감해 보인다.
* flip이 인터럽트 문맥에서 일어나므로, 모드 간 flip 횟수 차이(30초에
  ~3.1k 대 ~16.7k)는 이 인터럽트 문맥 경로가 틱마다 수행하는 작업량
  또는 트랩 여부의 차이로 보인다. 느린 모드에서는 flip마다
  INT3+HLE+재패치가 틱 처리 시간에 더해져 틱당 비용이 커진다.
* 이전 판의 "전량 재패치의 prologue 복원이 리타이어로 닫힌 슬롯을
  되살려서 빠르다"는 추정은 **수정**한다: flip 지점의 INT3는
  리타이어 폐쇄가 아니라 슬롯 자체의 가드 폴백(설계된 경로)이며,
  차이는 복원 여부가 아니라 슬롯 경로에 들어오는 빈도다. 복원
  부수효과 자체는 코드상 실재하므로(패처가 prologue를 다시 씀) 재패치
  축소 설계는 여전히 이를 고려해야 한다.

### v3 재판정(저녁, 통제 프로토콜): 역시 기각

같은 날 저녁, 기계가 안정적 빠른 영역(기준선 연속 15.7/17.6/19.6초)에
들어온 뒤 v3을 재판정했다. 두 exe를 같은 디렉터리에 사본으로 두고
워밍업 1회를 버린 뒤 교대 측정했다:

| 순서 | 빌드 | 공백 | handled loads |
|---|---|---|---|
| 1 | v3 | 29.8초 | 55.7k |
| 2 | 기준선 | 20.0초 | 45.4k |
| 3 | v3 | 25.4초 | 51.8k |
| 4 | 기준선 | 14.1초 | 43.6k |

**쓰기 동작이 바이트 단위로 동일한 v3이 일관되게 ~10초 나쁘다.**
재패치 경로를 빠르게 만드는 변경(마스크 v1·v2, 보호 범위 v3)이 모두
공백을 악화시킨다는 규칙이 독립적으로 재확인됐다. v3도 폐기했다.

### 모드 선택자에 대한 추가 배제

* **EEPROM 내용이 아니다**: 느린 런 직후의 `nvram\pumpitea\eeprom.dat`
  와 신선한 실행 한 번 뒤의 파일이 128바이트 전부 동일했다.
* 5시간 유휴 뒤 첫 실행은 느렸고(45.6초) 직후 연속 실행들은
  빨랐다(19.6/17.6/15.7초) — 유휴 후 첫 실행 효과는 존재하지만,
  오전의 교대 측정(연속 실행에서 빌드별로 갈림)은 설명하지 못한다.

## 미확정

* 모드(슬롯 경로 대 비슬롯 경로)를 가르는 정확한 분기. 실행 초기
  어떤 시점·조건에서 memcpy 블록이 캐시 상주가 되는지, 빠른 모드의
  flip이 정확히 어떤 경로로 처리되는지(privileged 0xC0000096 ~22만
  건/90초가 양 모드에 존재).
* **재패치를 빠르게 하면 느려지는 역설의 기전.** v1·v2·v3이 모두
  같은 방향으로 재현했으므로 우연이 아니다. 판별된 것:
  - **틱 병합 가설은 기각** — v3 재판정 4회의 틱 전달 카운터가
    빌드와 무관하게 동일했다(due 20.1k~20.6k/90초, coalesced 0).
  - **근접 원인은 guarded 폴백 증가** — v3은 guarded load 폴백이
    10.9k~13.3k로 기준선(6.5k~7.7k)보다 ~70% 많다. 기준선의 handled
    load 43k~45k는 틱 20.3k × ISR의 DS 올림/복원 2회와 거의 일치하고,
    v3의 초과분이 memcpy flip의 가드 실패다. 즉 "재패치가 느릴수록
    flip 가드가 더 자주 성공"하는 결합이 있다.
  - 뿌리 질문이었던 **가드 성공의 물리적 경위는 전용 probe로
    확정했다**(아래).

### VEH 재개는 컨텍스트의 세그먼트 쓰기를 무시한다 (`--segment-restore`)

`repiu_aot_probe --segment-restore`: INT3 핸들러가 재개 컨텍스트의
`SegEs`에 후보 selector를 넣고 재개한 뒤 물리 ES를 읽는다. 결과
(Win32 x86, Windows 10):

| 요청 | 관측된 물리 ES |
|---|---|
| 0x002B(대조) / 0x0000 / 0x0053 / 0x0090 / 0x002C / 0x0024 | **전부 0x002B** |

즉 **물리 세그먼트 레지스터는 예외 이전 값을 유지하고, 재개
컨텍스트의 `SegEs` 쓰기는 효과가 없다**(유효한 0x0053조차). 따라서:

* HLE 세그먼트 로드의 `win32_context->SegEs = selector`
  (`RecordGuestSegmentLoad`, `HandleDosGetInterruptVector`)는 물리
  레지스터에 아무 영향도 주지 않는 죽은 기록이다.
* guarded 슬롯의 `물리 == 새 값` 비교는 **flat(0x2B) 값 로드에서만
  통과할 수 있고, 게스트 selector(0x24 등) 로드는 구조적으로 항상
  폴백한다.** 이것이 flip의 제거 불가능한 트랩 비용의 정확한
  정의이며, issue 방향 3(shadow 기반 처리)이 없애야 할 대상이다.
* v3 역설의 폴백 증가는 0x2B 로드의 성공 조건인 `shadow == 0x2B`의
  타이밍 — 재해석의 shadow 재기록(`BuildAotSegmentTable`)과 슬롯
  실행 사이의 경주 — 와 결합된 것으로 좁혀진다.
* Win32에서 guarded load 네이티브 성공(빠른 모드에서 다수)의 정확한
  경위 — 슬롯은 물리 segment 레지스터와 비교하는데 물리 값이 게스트
  selector와 일치하는 경로가 무엇인지.
* 마스크 v1 + 트레이스 1회에서 관측한 게스트 AV(EIP `0x041E4ECF`,
  EBX=0) 종료. 재현 안 됨.

### 방향 3 구현 결과 (같은 날 저녁): 공백 절반 이하

설계 씨앗을 구현했다(`docs/design/20261006-i018-shadow-authoritative-
segment-load.md`): i386 guarded load 슬롯에서 물리 비교를 버리고
shadow를 진실원으로 삼아, `[shadow]`/레지스터별 수용 쌍
`[pair0]`/`[pair1]`과 비교해 쌍 일치 시 shadow를 네이티브로 쓴다.
교대 측정(90초):

| 빌드 | 공백 | guarded load 폴백 |
|---|---|---|
| 기준선 ×2 | 14.71 / 15.64초 | 6,873 / 7,394 |
| 방향 3 ×3 | **7.59 / 7.08 / 6.77초** | 2,525 (결정적) |

memcpy flip의 트랩·재해석·전량 재패치가 소멸했고, v1~v3의 "재패치를
싸게 하면 느려지는" 역설은 나타나지 않았다 — 트랩 자체가 사라져
되먹임 고리의 모양이 바뀌었기 때문으로 본다. 남은 트랩은 ISR의
메모리-소스 DS 로드(`mov ds, cs:[abs]`)와 guarded pop(폴백
48.7k/90초)이며, 이 둘이 5.0초(v0.0.180)와의 남은 간격의 후보다.

간헐 크래시 1건: pumpit3a 30초 스모크 4회 중 1회가 로딩 중 게스트
AV(EBX=0, `cmp dword [ebx+0xC], 0x1000`, EAX=0xDE1)로 종료 — 오전
마스크 v1 트레이스 런에서 1회 본 것과 동일 시그니처. 재시도 3회와
트레이스 런, 기준선 런은 정상. 세그먼트 경로의 타이밍이 바뀔 때
드러나는 기존 경합으로 추정하며 미확정에 남긴다.

### 방향 3 2단계 (2026-10-07): pop·메모리-소스 로드도 shadow 진실원으로

실행 파일 덤프로 남은 트랩을 확정한 뒤(위 "ES flip의 정체" 절), i386
guarded pop 슬롯을 같은 shadow·수용 쌍 형태로 바꾸고 메모리-소스
`mov Sreg, r/m16`을 guarded load 슬롯으로 받았다
(`docs/design/20261007-i018-shadow-authoritative-pop-and-memory-load.md`).
교대 측정(90초, 기준선 = 1단계 HEAD 빌드):

| 빌드 | 로고 뒤 공백 | handled segment load | guarded pop 폴백 | 재패치 지점 기록 합 |
|---|---|---|---|---|
| 1단계 기준선 ×2 | 3.51 / 2.99초 | 94,388 / 71,821 | 68,462 / 45,891 | 35.7M |
| 2단계 ×3 | **2.23 / 2.37 / 2.09초** | 25,888~25,996 | **3** | ~1만 |

* 틱 경로(ISR 진입 로드 + 말미 pop ds)와 memcpy 복원 pop의 INT3·재해석·
  전량 재패치가 소멸했다. handled segment load가 틱·memcpy 수에 비례하지
  않고 런 간 ±0.4%로 고정된다.
* 공백은 v0.0.180의 5.0초보다 짧다. 단, 오늘 기계는 어제 저녁(1단계 측정
  6.8~7.6초)보다 빠른 영역에 있었다 — 모드 분기 미확정 항목은 그대로다.
* 남은 guarded load 폴백 2,530은 `0xFCF6F` far 문자열 비교 관용구다.
* 두 번째 정지(41~43초, 1.8~4.4초)가 양쪽 빌드에 모두 있다 — 이번 경로와
  무관, 미확정에 추가.

## 다음 방향

1. **모드 분기 규명이 최우선이다.** 빠른 모드를 안정적으로 선택하게
   만들면 코드 수정 없이도 공백이 15~19초로 수렴하고, 그 다음 축소
   작업의 기준도 생긴다. 시작점: flip 슬롯(게스트 `0xFE376~0xFE38A`)이
   언제 번역·상주되는지, 빠른 모드에서 같은 명령의 처리 경로 추적.
2. **flip 자체 제거**(issue 방향 3)는 1단계(레지스터 로드)와 2단계(pop·
   메모리-소스 로드)로 구현됐다. 남은 것은 `0xFCF6F` 그룹(2,530/90초,
   앞 HLE 로드 뒤의 비슬롯 경로)과 41~43초의 두 번째 정지(양쪽 빌드
   공통, 원인 미확정)다.
3. 측정 규율: 이 공백의 A/B는 **식힌 기계에서 기준선을 교대로
   끼워서만** 판정한다. 장시간 연속 측정 후반의 수치는 버린다.
4. 게임이 스스로 기다리는 시간(보드 설정 delay 100~500ms × 횟수 +
   delay(5000) + calibrate의 실제 1초×N)은 실기에도 있는 하한이다.
   공백 목표를 세울 때 이 하한을 먼저 계산에 넣는다.

---

# pumpitea loading gap: segment flip and re-patch coupling

Facts accumulated while chasing issue #18's loading-gap regression. All
measurements: Win32 Release, `REPIU_STALL_TIMEOUT_MS=0`,
`REPIU_EXECUTION_TIMEOUT_MS=90000`, `REPIU_GLIDE_FRAME_RATE_LOG=1`,
window visible, default vsync (2026-10-06, Intel HD 620 laptop).

## Confirmed

* **The ES flip is the C runtime memcpy idiom** at guest `0xFE375..0xFE38A`:
  `push es; mov ax, ds; mov es, ax (0xFE378); … rep movsd; rep movsb;
  pop edi; pop es (0xFE38A)`. The virtual DS is `0x0024` and the saved ES
  `0x002B`, so every memcpy changes ES twice — through a guarded **load**
  site and a guarded **pop** site. (Re-dumped from the mounted executable
  on 2026-10-07; the earlier "`mov es, [saved]`" reading of `0xFE38A` was
  a guess and is corrected.) The tick path runs the same shape on DS: the
  INT8 ISR entry helper `0xFE2F0` is `66 2E 8E 1D disp32` (`mov ds,
  cs:[abs]` with an operand-size prefix, a memory-source load no slot
  accepted), and the ISR epilogue `0x2AB80..0x2AB87` is `pop gs; pop fs;
  pop es; pop ds; popad; iretd` — four guarded pop sites per tick. The
  run-invariant 2,520 group at `0xFCF6F` is a far-string compare idiom,
  `mov ds, dx; mov es, ax (0xFCF6F); repe cmpsb; …; pop es; pop ds`.
* **Both selectors fold base 0**: `0x002B` (host flat, native) and
  `0x0024` (limit `0xF76BB`, under the DOS low-memory bound, so override
  sites route to HLE). A re-patch changes only the guard immediates and
  the native/HLE routing form.
* **Why v0.0.180 avoided this**: 180's Win32 did not track a virtual DS,
  so `mov ax, ds` read the physical flat `0x2B` and the following
  `mov es, ax` was a no-op load that the guard passed natively (75,905
  successes / 2,532 fallbacks). The Linux x64 port made DS a tracked
  virtual selector (`0x0024`), which is more faithful to real DOS/4GW;
  180's behavior was an accidental shortcut.
* **The whole-cache re-patch is not pure waste** (issue directions 1 and
  2 are refuted). Same-day interleaved 90 s runs: baseline (v0.0.205)
  gaps **18.7 / 15.0 / 19.6 s**; masked v1 (override by changed
  resolution, guarded by shadow address) **35.1 / 45.5 / 45.2 s**;
  masked v2 (both by changed resolution) **52.6 / 60.8 s**. The third
  baseline ran right after v2, excluding thermal drift. Guarded-load
  native successes collapsed (baseline ~90% success rate → v1 ~23%) and
  handled segment loads grew 2.5x, although per-re-patch site writes
  dropped to 1/5–1/12.
* **The loading phase is exception-dispatch bound**: a 30 s census on
  the masked build counted 296,966 exceptions — 209,722 breakpoints,
  61,247 privileged (0xC0000096), 23,716 access violations. The top
  breakpoint sites are `0x04101297` (50,498), `0x04101BAE` (44,983),
  `0x041012D3` (35,349), then the flip trio at 16,741 each.
* **Guest-page retirement closes 1,466 address-map entries with INT3**
  early in both builds (`retire guard reset #1 page=0x04107000
  entries=1466`, generation 227).
* **The hot breakpoint sites are DOS calls, not re-entries**: the top
  of the 30 s breakpoint census is `calibrate()`'s one-real-second
  counting loop on INT 21h AH=2Ch (`0x04101297`), the `delay(ms)` spin
  (`0x041012D3`), the lseek wrapper AH=42h (`0x04101BAE`) and the
  calibrate's second-edge sync — the flip slots trail at 3,139 hits
  (fast mode) against 16,741 (slow mode). Disassembly: `calibrate()` at
  guest `0x101277` counts AH=2Ch calls for one real second into
  `[0x047E04A8]`; `delay(ms)` at `0x1012A9` spins `ms×rate/1000` calls.
  Its six callers are one `delay(5000)` in init and `delay(100..500)`
  after I/O-board port writes (0x2A4/0x2AC/0x2DA) — the game waits on
  itself after each board setup, and `HandleDosGetSystemTime` returns
  the real wall clock.
* **Time budget** (`REPIU_EXECUTION_TIME_PROFILE=1`, baseline 30 s):
  VEH 65.22% of wall across 283,208 exceptions; inside the VEH bucket,
  prologue 0.59% / aot-transfer 7.09% / hle-chain 12.35% /
  **residual 79.81%**. The host-site symbol attribution puts the
  residual mostly in the re-patch path: baseline `ProtectMemory` 29.5%
  + guarded-load patcher 17.6% + `ReResolveWin32AotSegmentOverrides`
  8.9% + pop/override patchers ≈ **62% of sited host samples**; masked
  v1 still sums ≈56% because its flip-trap count grew ~5x.
* **Runs are bimodal and the machine picks the mode.** Fast mode: few
  flip-slot traps, guarded successes dominate, gap 15–19.6 s. Slow
  mode: every flip falls back through the slot, gap 32–61 s. The
  morning interleave put all three baselines in fast mode and all three
  masked v1 runs in slow mode; after ~2 h of continuous load an
  **unmodified baseline measured 40.2 s**, so machine state (thermal
  throttling suspected) also selects the mode. Hence: v1's regression
  stands (interleaved), v2 is bracketed by a 19.6 s baseline, and
  **v3 (write-identical, `ProtectMemory` capacity→size) is
  inconclusive** — its 32–44 s matches the drifted 40.2 s baseline.

* **The flips run in interrupt-context code** (full 20 s trace with
  `REPIU_DPMI_SEGMENT_TRACE=1`, 14,151 segment loads): the INT8 ISR at
  guest `0x2AAE4` raises DS to `0x0024` through the helper `0x0FE2F0`
  (`mov ds, cs:[0x0FE2F9]`) and restores the interrupted `0x002B` at
  `0x2AB85`; every one of the run's 837 memcpy ES flips executed with
  `ds=0x0024` between such an entry and its restore. The mainline DS
  stays `0x002B`, which refutes the interim "mainline DS absorption"
  hypothesis.

* **Pre-state of every flip trap** (extended trace: physical Seg* and
  pre-update shadow words added to `TraceGuestSegmentEvent`, traced
  before any state write; unwrapped 25 s capture): the `0x0024` load at
  `0xFE378` always sees physical 0x002B ≠ new — structurally always a
  fallback; the flat reload at `0xFE38A` always sees shadow 0x0024 left
  by the preceding HLE — also always a fallback. So one guest memcpy
  costs at least two fallbacks, two ES re-resolutions and two
  whole-cache re-patches by construction, and the guarded-success
  counters come from other sites (same-selector reloads such as the
  0xFCF6x trio — which trapped 2,520 times with an all-match pre-state,
  i.e. in a non-native slot form, at run-invariant counts). Design seed
  for direction 3 that falls out directly: a slot that writes the
  shadow word natively when only the shadow mismatches (flat reload),
  plus one per-register accepted-alternate selector for the guest-
  selector side, removes the flip's traps entirely; it requires moving
  the segment state's source of truth to the shadow block so HLE
  boundaries stop reading a stale `context->guest_*`.

## Inferred

* Slow mode is the memcpy flip running through the AOT cache slot path
  (guard fallback INT3 → HLE → re-resolution → whole-cache re-patch);
  in fast mode the same instructions take another path and the slot
  fallbacks and re-patches mostly do not happen. Mode selection looks
  sensitive to early-run timing (dynamic translation completion, thread
  scheduling, throttle state).
* Since the flips are interrupt-context work, the mode difference in
  flip counts (~3.1k against ~16.7k per 30 s) reflects how much that
  per-tick path does or whether it traps; in slow mode each flip adds
  INT3+HLE+re-patch to every tick's cost.
* The earlier inference — that the full re-patch's prologue restore
  resurrects retirement-closed slots and that this is what the masked
  builds lost — is **revised**: the flip-site INT3s are the slots' own
  designed guard fallbacks, not retirement closures. The restore side
  effect does exist in the patchers and still constrains any re-patch
  reduction, but the measured differential tracks slot-path frequency,
  not restoration.

* **v3 re-judged the same evening under a controlled protocol and
  rejected too**: with the machine back in a stable fast regime
  (baselines 15.7/17.6/19.6 s), side-by-side exe copies, one discarded
  warm-up and alternation, v3 measured 29.8/25.4 s against contemporaneous
  baselines of 20.0/14.1 s. A write-identical change that only narrows
  `ProtectMemory` to `size` is consistently ~10 s worse, independently
  reconfirming that speeding the re-patch path (masks v1/v2, protection
  range v3) lengthens the gap.
* **Mode-selector exclusions**: the EEPROM contents are not it (the
  file after a slow run and after one fresh run are byte-identical);
  a first-run-after-idle slowdown exists (45.6 s once, then
  19.6/17.6/15.7 s) but cannot explain the morning's per-build
  alternation.

## Unresolved

* The exact bifurcation that picks the mode, and the fast-mode handling
  path of the flip instructions (privileged 0xC0000096 ≈220k per 90 s
  exists in both modes).
* **The mechanism of the speed-up-makes-it-slower paradox.** v1, v2 and
  v3 all reproduced it in the same direction, so it is not chance.
  Discriminated so far: tick coalescing is excluded (tick delivery was
  identical across the four re-judgment runs — due 20.1k–20.6k per
  90 s, coalesced 0); the proximate cause is guarded-load fallbacks
  growing ~70% under v3 (10.9k–13.3k against 6.5k–7.7k), while the
  baseline's handled loads match ticks × the ISR's two DS loads almost
  exactly. The root question — the physical path of a guard success —
  is settled by a dedicated probe (below).
* **A VEH resume ignores segment writes in the context**
  (`repiu_aot_probe --segment-restore`): resuming with 0x0000, 0x0053,
  0x0090, 0x002C or 0x0024 in `SegEs` leaves the physical ES at its
  pre-exception 0x002B in every case. Hence the HLE's
  `win32_context->SegEs = selector` is a dead write; a guarded slot's
  physical-against-new compare can only pass for flat-value loads, so
  **guest-selector loads structurally always fall back** — the exact
  trap cost issue direction 3 (shadow-based handling) must remove; and
  the v3 fallback growth narrows to the timing of `shadow == 0x2B` at
  the flat reload (a race between the re-resolve's shadow rewrite and
  slot execution).
* How guarded loads succeed natively on Win32 in fast mode, given the
  slot compares the physical segment register.
* One crash under the resolution trace on masked v1 (guest AV at
  `0x041E4ECF` with EBX=0); not reproduced.

* **Direction 3 implemented the same evening — the gap halves.** The
  i386 guarded load slot drops the physical compare and treats the
  shadow as the source of truth, comparing against the shadow and the
  per-register accepted pair and writing the shadow natively on a pair
  match. Interleaved 90 s runs: baselines 14.71/15.64 s against
  **7.59/7.08/6.77 s**, guarded-load fallbacks deterministic at 2,525.
  The flip's traps, re-resolutions and whole-cache re-patches are gone
  and the v1–v3 paradox did not reappear (the trap itself is removed,
  which changes the feedback loop's shape). Remaining traps: the ISR's
  memory-source DS load and the guarded pop (48.7k fallbacks per
  90 s) — the candidates for the rest of the distance to v0.0.180's
  5.0 s. One intermittent crash: pumpit3a ended one of four 30 s smokes
  with the same guest-AV signature seen once this morning (EBX=0 at
  `cmp dword [ebx+0xC], 0x1000`); three retries and a traced run were
  clean — left unresolved as a suspected pre-existing race exposed by
  segment-path timing changes.
* **Direction 3 phase 2 (2026-10-07): pop and memory-source loads.** With
  the remaining sites settled from the executable dump, the i386 guarded
  pop slot took the same shadow/accepted-pair form and memory-source
  `mov Sreg, r/m16` became a guarded load slot. Interleaved 90 s runs
  against the phase-1 build: post-logo gap 3.51/2.99 s → **2.23/2.37/
  2.09 s**, guarded-pop fallbacks 68,462/45,891 → **3**, handled segment
  loads 94k/72k → 26k (run-invariant), whole-cache re-patch site writes
  35.7M → ~10k. The tick path and the memcpy restore no longer trap. The
  machine sat in a faster regime than during phase 1 (6.8–7.6 s), so the
  mode question stays open; the gap is below v0.0.180's 5.0 s. Still
  unresolved: the `0xFCF6F` group (2,530 per 90 s) and a second
  1.8–4.4 s stall at 41–43 s present in both builds.

## Next directions

1. **Identify the mode bifurcation first.** Making fast mode the
   reliable outcome converges the gap to 15–19 s with no code change
   and gives later reductions a stable floor. Entry point: when the
   flip slots (guest `0xFE376..0xFE38A`) become cache-resident, and
   what path fast mode uses for the same instructions.
2. Removing the flip itself (issue direction 3) is implemented in two
   phases (register loads; pops and memory-source loads). What remains
   is the `0xFCF6F` group (2,530 per 90 s, the non-slot path after a
   preceding HLE load) and the second stall at 41–43 s seen in both
   builds, cause unknown.
3. Measurement discipline: A/B on this gap is judged only with
   interleaved baselines on a cooled machine; late numbers from long
   continuous sessions are discarded.
4. The game's own waits (delay(100..500) per board write, delay(5000),
   calibrate's real second × N) are a floor that real hardware also
   paid; compute that floor before setting a gap target.
