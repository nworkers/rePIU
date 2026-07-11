# DOS/4G client GS/private environment 작업 지시

1. BW executable image 전체에서 GS load/restore를 분류한다.
2. DOS4GW client transition의 `GS=ES:[DI+0x0A]` context layout을 복원한다.
3. context `+0x0A` writer와 selector descriptor 생성자를 역추적한다.
4. selector image offset `0x42` writer와 far-pointer target을 찾는다.
5. `LINEXE_LOADER` module 및 export table population과 연결한다.
6. rePIU가 합성해야 하는 최소 selector/image bytes를 문서화한다.
7. 재현 가능한 trace, 검증, 빌드, 작업 로그와 커밋을 남긴다.

# DOS/4G Client GS/Private-Environment Work Order

Classify all GS loads, recover the DOS4GW client-transition context, trace its GS-field writer and selector descriptor, locate offset-`0x42` population, connect the `LINEXE_LOADER` module and exports, define the minimum synthetic selector/image, emit reproducible traces, verify, build, document, and commit.
