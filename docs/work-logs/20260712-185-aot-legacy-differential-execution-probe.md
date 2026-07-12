# AOT differential probe 작업 로그

legacy/AOT의 `0x000F8460` return entry를 비교했습니다. direct-call 직전 register와 stack은 일치했지만 두 번째 return target은 `1`로 달랐습니다. transfer trace와 cache map을 추가해, 원인이 call ABI가 아니라 cache 기본 블록 연결 누락임을 확인했습니다.

# AOT Differential Probe Work Log

Compared legacy and AOT at return entry `0x000F8460`. Registers and stack matched before the direct call, but the second return target was `1`. Added transfer tracing and cache-map inspection, which identified missing cache basic-block linking rather than the call ABI as the cause.
