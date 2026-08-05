# Task 433 작업 지시 — Glide 정점 깊이 연결

설계: [20260806-433](../design/20260806-433-glide-vertex-depth.md)

## 1. 범위

정점의 `ooz`를 OpenGL 깊이로 흘려보냅니다. 투영·비교 함수·cull mode는 건드리지 않습니다.

## 2. 변경할 파일

| 파일 | 내용 |
|---|---|
| `glide_vertex_depth_census.h/.cpp` | 어느 필드가 깊이를 싣는지 재는 census(신규, 동작 불변) |
| `glide_vertex.h` | `GlideDrawVertex`에 `ooz` 추가 |
| `glide_vertex.cpp` | `fields[6]` 디코드 + census 표본, meaningful 미만은 0으로 |
| `glide_opengl_backend.cpp` | `GlideOozToOrthoEyeZ`, `glVertex3f`의 z 인자 |
| `main.cpp` | census 요약, 깊이 버퍼 granted-bits 진단 |
| `CMakeLists.txt` | 새 소스 등록 |

**건드리지 않을 것:** `glOrtho` near/far, `glDepthFunc` 매핑, cull mode, W-buffer 모드,
LFB blit 경로(깊이 테스트를 스스로 끄고 복원함).

## 3. 구현 규칙

* 정점은 **`ooz`를 원본 그대로** 싣고, 정규화는 backend에서 합니다(플랫폼 분리).
* 매핑은 **단조 증가**여야 합니다. 그래야 `grDepthBufferFunction` → `glDepthFunc`
  직결이 비교 결과를 그대로 옮깁니다(설계 §2).
* meaningful 미만(`|v| ≤ 1e-6`)은 0으로 접습니다. 게스트가 안 쓴 필드는 깨끗한 0이
  아니라 denormal이라, 그대로 두면 범위 전체에 흩뿌려집니다.

## 4. 검증

1. 빌드 통과, 스모크에서 `unimplemented/unsupported = 0/0`, GL 오류 0.
2. census로 `ooz`가 깊이임을 확인(완료: gameplay 표본 95.4%, 범위 0.75~65,426).
3. **사용자 육안 확인** — gameplay에서 3D 모델 깨짐이 사라졌는지. 이것만이 완료 판정입니다.

---

# Task 433 Work Order — connect Glide vertex depth

## 1. Scope

Carry the vertex `ooz` through to OpenGL depth. The projection, comparison mapping and cull mode
are left alone.

## 2. Files

A new vertex depth census (behaviour-neutral) that measures which field carries depth; `ooz` on
`GlideDrawVertex`; the decoder reading `fields[6]` and folding sub-meaningful values to zero;
`GlideOozToOrthoEyeZ` and the `glVertex3f` z argument in the backend; the census summary and the
granted-depth-bits diagnostic in `main.cpp`; and the CMake registration. **Not touched:** the
`glOrtho` near and far planes, the `glDepthFunc` mapping, cull mode, W-buffer modes, and the LFB
blit path, which disables and restores depth testing itself.

## 3. Implementation rules

The vertex carries **raw `ooz`** with normalisation done in the backend, keeping the
platform-specific mapping out of the HLE. The mapping must be **monotonically increasing**, which
is what lets the direct `grDepthBufferFunction` to `glDepthFunc` mapping carry comparison results
across unchanged (design section 2). Values below the meaningful floor fold to zero, because a
field the guest never wrote reads back as denormal rather than clean zero and would otherwise
scatter through the range.

## 4. Verification

The build passes with a smoke showing `unimplemented/unsupported` at 0/0 and no GL errors; the
census confirms `ooz` as the depth field (done: 95.4% of gameplay vertices, range 0.75-65,426);
and **the user confirms visually** that the 3D model corruption is gone in gameplay — which is
the only thing that closes this.
