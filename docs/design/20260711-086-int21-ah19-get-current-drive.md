# INT 21h AH=19h 현재 드라이브 조회 설계

## 배경

`INT 21h AH=47h` 현재 디렉터리 조회를 통과한 뒤 `piu_1st`는 relocated base + `0x000F423B`의 `CD 21`에서 중단된다. 당시 `EAX=0x00001900`이므로 요청은 DOS `INT 21h AH=19h`, 현재 기본 drive 조회이다.

바로 앞 흐름은 `AH=47h`로 받은 `DATAS\BGA` 앞에 drive 문자와 `\`를 조립하려는 코드로 보인다. 따라서 현재 drive 조회도 DOS 경로 문자열 조립에 필요한 최소 HLE로 처리한다.

## 설계

현재 HLE는 drive별 filesystem table을 갖지 않고 target root 하나를 단일 DOS drive처럼 사용한다. 이 단일 가상 drive는 `C:`로 노출한다.

* `AH=19h`는 `AL=2`를 반환한다.
* DOS drive 번호는 `A=0`, `B=1`, `C=2` 형식으로 기록한다.
* carry flag는 clear한다.
* 일반/traced `INT 21h` handler 양쪽에 동일한 helper를 연결한다.
* 처음부터 전용 로그 필드를 추가해 반환 drive를 확인할 수 있게 한다.

## 기대 결과

`piu_1st`는 현재 drive 조회를 통과하고 다음 HLE 요구사항으로 이동해야 한다. 로그에는 `Win32 last DOS get drive value: 0x02`가 표시되어야 한다.

## 범위 밖

* drive별 current directory table
* host drive letter와 DOS drive letter 매핑
* `AH=0Eh` default drive 설정

# INT 21h AH=19h Get Current Drive Design

## Background

After passing `INT 21h AH=47h` get current directory, `piu_1st` stops at `CD 21` at relocated base + `0x000F423B`. `EAX=0x00001900`, so the request is DOS `INT 21h AH=19h`, get current default drive.

The surrounding code appears to build a drive letter and `\` prefix before the `DATAS\BGA` path returned by `AH=47h`. The current drive query is therefore handled as the minimal DOS HLE needed for path-string construction.

## Design

The current HLE does not maintain per-drive filesystem tables and uses one target root as a single DOS drive. Expose that single virtual drive as `C:`.

* `AH=19h` returns `AL=2`.
* DOS drive numbering is recorded as `A=0`, `B=1`, `C=2`.
* Clear carry flag.
* Wire the same helper into both normal and traced `INT 21h` handlers.
* Add dedicated logging from the start so the returned drive can be observed.

## Expected Result

`piu_1st` should pass the current drive query and move to the next HLE requirement. The log should show `Win32 last DOS get drive value: 0x02`.

## Out Of Scope

* Per-drive current directory tables
* Host drive letter to DOS drive letter mapping
* `AH=0Eh` default drive selection
