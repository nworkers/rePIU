# Task 675 작업 로그: Linux x64 segment store linear resolution

## 한국어

`8C /r` segment store의 memory form을 공통 HLE handler에서 decode하고,
selector table의 descriptor base와 guest offset으로 linear destination을
계산하도록 했습니다. `66 8C`와 `66 26 8C` absolute ES form도 같은 정책을
사용합니다.

실행 trace에서 selector `0x0000` store가 guest 주소 `0x011A6638`에 기록되는
것을 확인했고, 이전에 HLE에서 누락되던 `66 8C` memory form을 통과시켰습니다.

## English

The common HLE handler now decodes memory forms of `8C /r` segment stores and
computes the linear destination from the selector-table descriptor base plus
the guest offset. `66 8C` and the `66 26 8C` absolute-ES form use the same
policy.

Runtime trace confirmed a selector `0x0000` store at guest address
`0x011A6638`, and the previously missing `66 8C` memory form passed through
HLE.
