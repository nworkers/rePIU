# 20260802-395 AOT-DBT unresolved direct-edge dispatch 작업 지시

설계: [20260802-395-aot-dbt-unresolved-direct-edge-dispatch.md](../design/20260802-395-aot-dbt-unresolved-direct-edge-dispatch.md)

## 한국어

1. 공용 code-cache option과 direct-edge dispatch site metadata를 추가합니다.
2. unresolved direct fixup을 image 끝의 fail-closed stub으로 변환합니다.
3. Win32 전용 host-stack thunk와 runtime resolver를 별도 파일로 구현합니다.
4. static placement, breakpoint provenance/target 복원, dynamic append metadata를
   연결합니다.
5. synthetic probe를 추가하고 전체 probe를 실행합니다.
6. pumpit1 회귀와 pumpit2 AOT-DBT smoke를 검증합니다.
7. analysis, architecture, frontier, work log를 갱신하고 커밋합니다.

## English

1. Add the shared build option and direct-edge dispatch-site metadata.
2. Convert unresolved direct fixups into fail-closed tail stubs.
3. Implement a dedicated Win32 host-stack thunk and runtime resolver.
4. Wire static placement, breakpoint target recovery, and dynamic append.
5. Add a synthetic probe and run the full probe suite.
6. Verify pumpit1 regression and a pumpit2 AOT-DBT smoke.
7. Update analysis, architecture, frontier, and work log, then commit.
