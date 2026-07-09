# loader 로그 포맷 작업 로그

## 결과

Win32 loader 로그 포맷을 요청한 형식으로 변경했다.

변경 내용은 다음과 같다.

* logger pattern을 `[%X.%e] [%8l] [%n] %v`로 변경했다.
* timestamp는 `HH:MM:SS.mmm` 형태로 출력된다.
* level 필드는 8칸 오른쪽 정렬로 출력되어 `info`, `warning`, `error`가 같은 폭으로 보인다.
* 기존 warn/error 분류 정책과 실행 동작은 변경하지 않았다.
* `ARCHITECTURE.md`의 Win32 loader log level policy에 로그 pattern을 기록했다.

## 검증

`powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`

결과: 성공.

출력 예시는 다음 형태로 확인했다.

```text
[15:44:34.167] [    info] [loader] Win32 loader policy: valid
[15:44:34.167] [ warning] [loader] Win32 host range available: false
[15:44:34.361] [   error] [loader] Win32 minimal execution exception caught: true
```

`dos4gw_hello`와 `piu_1st` target 모두 기존 검증 기준을 통과했다.

## 다음 작업

다음 구현 작업은 기존과 동일하게 `26 8A 4F FF` segment-override byte memory load를 selector shadow state와 주소 변환 정책으로 처리하는 것이다.

# Loader Log Format Work Log

## Result

Changed the Win32 loader log format to the requested shape.

Changes:

* Changed the logger pattern to `[%X.%e] [%8l] [%n] %v`.
* Timestamps are printed as `HH:MM:SS.mmm`.
* The level field is right-aligned in an 8-character field, so `info`, `warning`, and `error` share the same width.
* Preserved the existing warn/error classification policy and execution behavior.
* Recorded the log pattern in the Win32 loader log level policy in `ARCHITECTURE.md`.

## Verification

`powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`

Result: success.

Confirmed output in the following shape:

```text
[15:44:34.167] [    info] [loader] Win32 loader policy: valid
[15:44:34.167] [ warning] [loader] Win32 host range available: false
[15:44:34.361] [   error] [loader] Win32 minimal execution exception caught: true
```

Both `dos4gw_hello` and `piu_1st` targets passed the existing verification criteria.

## Next Work

The next implementation task remains handling the `26 8A 4F FF` segment-override byte memory load through selector shadow state and address translation policy.
