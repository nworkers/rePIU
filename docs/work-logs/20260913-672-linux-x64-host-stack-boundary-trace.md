# Task 672 작업 로그: Linux x64 host stack boundary trace

## 한국어

guest entry, cache call, return thunk에서 host `RSP`를 저장하는 opt-in trace를
추가했습니다. fault 시점의 값은 다음과 같이 확인되었습니다.

```text
entry_rsp=0x7f52dffbecb8
cache_rsp=0x7f52dffbec80
thunk_rsp=0xdffbec7c
```

guest entry와 cache call은 정상적인 high host address였지만 return thunk 시점은
guest ESP처럼 낮아졌습니다. 이 결과로 문제 범위를 generic cache-boundary
reentry로 좁혔습니다.

## English

Added an opt-in trace that records host `RSP` at guest entry, cache call, and
return-thunk boundaries. A fault report showed:

```text
entry_rsp=0x7f52dffbecb8
cache_rsp=0x7f52dffbec80
thunk_rsp=0xdffbec7c
```

Guest entry and cache-call RSP were valid high host addresses, while the
return-thunk observation had become guest-ESP-like. This narrowed the fault to
generic cache-boundary reentry.
