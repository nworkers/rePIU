# DOS/4G service 0 frame/data-flow 작업 지시

1. resident INT 21h entry부터 primary handler return까지 control flow를 복원한다.
2. stack push 순서와 24-byte context copy의 source/destination layout을 계산한다.
3. restore path를 역추적해 frame offset별 register/flags 의미를 검증한다.
4. `AX=FF00h`, `DX=0078h`를 primary와 secondary dispatch에 전파한다.
5. 반환 `EAX/AL`, `GS`, flags 및 private environment selector의 source를 기록한다.
6. 결과를 분석 문서와 재현 가능한 machine-readable trace로 남긴다.
7. 검증, 빌드, 작업 로그와 커밋을 수행한다.

# DOS/4G Service-Zero Frame/Data-Flow Work Order

Recover control flow, derive and verify every saved-frame field from both save and restore paths, propagate the identification-call inputs through primary and secondary dispatch, identify returned registers, flags, and private-environment selector provenance, emit reproducible analysis artifacts, verify, build, document, and commit.
