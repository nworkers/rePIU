# Glide GLSL combine pipeline 작업 지시

1. 공용 Glide alpha-combine state를 추가합니다.
2. Win32 OpenGL shader compiler/program/uniform을 별도 파일로 구현합니다.
3. WGL context 생성/해제와 shader 생명주기를 연결합니다.
4. `grAlphaCombine(1,0,0,2,0)` typed signature와 uniform 갱신을 구현합니다.
5. 빌드와 GUI 실행으로 다음 실제 Glide API까지 관찰합니다.
6. 분석·아키텍처·작업 로그를 갱신하고 커밋합니다.

# Glide GLSL Combine Pipeline Work Order

Add shared Glide alpha-combine state; implement Win32 shader compilation, program ownership, and uniforms in separate files; connect shader lifetime to WGL context lifetime; implement typed `grAlphaCombine(1,0,0,2,0)` uniform updates; build and run through the next real Glide API; update analysis/architecture/work logs; and commit.
