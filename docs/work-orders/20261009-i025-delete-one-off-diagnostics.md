# 작업 지시: 이미 답이 나온 질문을 위한 일회성 진단 삭제 (issue #25)

설계: `docs/design/20261009-i025-delete-one-off-diagnostics.md`

1. Glide 진단 6개 — `linexe_glide_boundary.cpp`(call audit, tex census 두 곳, draw census,
   tri census, frame dump 함수·전역·swap 호출, LFB BMP writer와 호출), 쓰지 않게 된
   `<fstream>`·`<filesystem>` include.
2. vertex depth census — `glide_vertex_depth_census.h/.cpp` 삭제, `glide_vertex.cpp`로 크기
   하한 이동, loader 출력, CMake.
3. `REPIU_LOWMEM_TRACE` — `execution_trampoline.cpp`.
4. `REPIU_AOT_PROBE_GUEST` — `execution_trampoline.cpp`, `thread_context.h`,
   `execution_trampoline.h`, loader 출력.
5. 문서: Glide analysis의 방법 메모, ARCHITECTURE 해당 없음 확인, 환경 변수 목록, TODO.
6. 검증: Win32·Linux x64 Release 빌드, aot_probe 체인, core probe, Glide probe, 변수 읽기
   `grep`.

완료 기준: 코드가 9개 변수를 더 읽지 않고, 유지한 4개는 판정 근거를 목록에 남긴다.

---

# Work order: delete one-off diagnostics for answered questions (issue #25)

(1) Six Glide diagnostics in the gate boundary, with the includes they alone used. (2) The vertex
depth census files, loader output and CMake entry, moving the decoder's magnitude floor into
`glide_vertex.cpp`. (3) `REPIU_LOWMEM_TRACE`. (4) `REPIU_AOT_PROBE_GUEST` with its result fields
and loader output. (5) The Glide analysis method notes, the inventory and the TODO. (6) Verify:
Win32 and Linux x64 Release builds, the aot_probe chain, core and Glide probes, `grep`. Done when
the code no longer reads the nine variables and the four kept ones have their reasons recorded.
