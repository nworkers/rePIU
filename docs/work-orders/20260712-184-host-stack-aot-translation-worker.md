# Host-stack AOT translation worker 작업 지시

1. ThreadContext에 고정 request/result와 Win32 events를 추가합니다.
2. guest thread 시작 전에 dynamic worker를 생성합니다.
3. VEH의 직접 append 호출을 event request/wait로 교체합니다.
4. guest 종료·timeout·실패 경로에서 worker를 shutdown/join합니다.
5. worker request/success/failure telemetry를 추가합니다.
6. PIU `aot-dynamic`에서 host exception 제거 여부와 새 frontier를 관찰합니다.
7. stable `aot`과 legacy 경로를 재검증합니다.
8. 문서화하고 커밋합니다.

# Host-Stack AOT Translation Worker Work Order

Add serialized request/result events and a Win32 translation worker, replace direct VEH appends, cleanly shut down on every exit path, add telemetry, observe PIU, reverify stable modes, document, and commit.
