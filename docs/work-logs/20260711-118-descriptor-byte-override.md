# descriptor-backed segment override byte 작업 로그

8비트 register read/write, 8비트 CMP arithmetic flags, descriptor-backed `ReadSegmentByte`를 추가했다. 관찰된 다음 형식을 처리한다.

* `26 3A 10`: `CMP DL,ES:[EAX]`
* `26 8A 30`: `MOV DH,ES:[EAX]`
* `26 80 38 00`: `CMP byte ptr ES:[EAX],0`
* `26 8A 10`: `MOV DL,ES:[EAX]`

```mermaid
flowchart LR
    B["Frontier +0xFC723"] --> D["Descriptor byte HLE"]
    D --> P["Progress +0xFC777"]
    P --> X["Further progress"]
    X --> S["In-process telemetry stops"]
```

## 검증과 판단

* Win32 x86 Debug 빌드 성공
* hello sample 정상 반환
* first pair 처리 후 last guest EIP가 `+0xFC723`에서 `+0xFC777`로 이동
* ES=`0x2C`, offset 0 byte read가 진단에 기록됨
* immediate compare까지 처리하면 실행이 10초 이상 계속되지만 in-process live snapshot이 시작 이후 갱신되지 않고 host 결과도 반환되지 않음

이 증거는 동일 프로세스 telemetry만으로 다음 정지를 안전하게 분류할 수 없음을 뜻한다. 다음 작업은 별도 supervisor와 process-external telemetry다.

# Descriptor-Backed Segment-Override Byte Work Log

Added 8-bit register helpers, complete byte-CMP arithmetic flags, descriptor-backed `ReadSegmentByte`, and observed ES-override register/immediate compare/load forms. Execution advances from `+0xFC723` to `+0xFC777`, recording an ES=`0x2C`, offset-zero byte read. After the immediate-compare form is enabled, execution continues beyond the prior frontier for more than ten seconds, but in-process live snapshots stop after startup and the host returns no result. This is evidence to move the next diagnostic boundary into an external supervisor process.
