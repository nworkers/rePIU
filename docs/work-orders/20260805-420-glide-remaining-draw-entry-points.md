# Task 420 작업 지시 — 남은 Glide draw 진입점

설계: [20260805-420](../design/20260805-420-glide-remaining-draw-entry-points.md)

## 1. 구현

| 파일 | 변경 |
|---|---|
| `include/repiu/hle/glide_vertex.h` | `kMaxGlidePolygonVertices = 64` |
| `include/repiu/platform/win32/glide_opengl_backend.h` | `DrawPoint`, `DrawPolygon` 선언 |
| `src/platform/win32/glide_opengl_backend.cpp` | 두 함수 구현 — `GL_POINTS` 1개, `GL_TRIANGLE_FAN` n개. 둘 다 host thread rendezvous 규약을 따름 |
| `src/platform/win32/boundary/linexe_glide_boundary.cpp` | point(+AA), AA line, AA triangle, polygon 6종 case. draw milestone 범위에 AA 구간 추가 |

**ABI 불변:** 각 case의 `Esp` 증가량은 `glide_hle.cpp` 서명표의
`argument_byte_count`와 일치해야 합니다.

| 진입점 | 인자 바이트 | `Esp` 증가 |
|---|---:|---:|
| `_GRDRAWPOINT@4` · `_GRAADRAWPOINT@4` | 4 | 2 slots |
| `_GRDRAWLINE@8` · `_GRAADRAWLINE@8` | 8 | 3 slots |
| `_GRAADRAWTRIANGLE@24` | 24 | 7 slots |
| `_GRDRAWPOLYGON@12` · `_GRDRAWPLANARPOLYGON@12` · `_GRAADRAWPOLYGON@12` | 12 | 4 slots |
| `_GRDRAWPOLYGONVERTEXLIST@8` · `_GRDRAWPLANARPOLYGONVERTEXLIST@8` · `_GRAADRAWPOLYGONVERTEXLIST@8` | 8 | 3 slots |

## 2. 빌드

```powershell
cmd /c scripts\build_win32_x86_release.bat
```

## 3. 검증

| 항목 | 방법 | 통과 |
|---|---|---|
| ABI·디코드 | `repiu_glide_render_probe` | 전부 통과 |
| 기능 | pumpit2 60초 — `GLIDE_UNIMPLEMENTED_FUNCTION`에서 `_GRDRAWPOINT@4`가 사라지고 `unimplemented` 카운터 2 → **0** | |
| 회귀 | pumpit3 60초 — 프레임이 Task 419 수준(약 3,036~3,084) 유지 | |
| 정확성 | 새 decline 사유 0건, `frame-errors=0`, GL 에러 0 | |

**프레임 증가를 기대하지 않습니다.** 누락된 그리기의 복원이며 호출 수가 2회이므로
성능 영향은 잡음 이하입니다.

## 4. 완료 기준

1. 빌드와 probe 통과.
2. pumpit2에서 `unimplemented` 카운터 0.
3. pumpit3 회귀 없음.
4. 작업 로그 작성, `ARCHITECTURE.md`의 Glide 절과
   [glide2x-ovl-and-opengl-hle](../analysis/glide2x-ovl-and-opengl-hle.md) 갱신.

---

# Task 420 Work Order — the remaining Glide draw entry points

Design: [20260805-420](../design/20260805-420-glide-remaining-draw-entry-points.md).

## 1. Implementation

`kMaxGlidePolygonVertices` in `glide_vertex.h`; `DrawPoint` and `DrawPolygon` on the backend
(one vertex with `GL_POINTS`, n with `GL_TRIANGLE_FAN`, both following the host-thread
rendezvous protocol); and boundary cases for point and AA point, AA line, AA triangle, and the
six polygon forms, with the AA range added to the draw milestone.

**The ABI must not move**: each case's `Esp` adjustment matches `argument_byte_count` in
`glide_hle.cpp` — two slots for the `@4` points, three for the `@8` lines and vertex lists,
four for the `@12` polygons, and seven for `_GRAADRAWTRIANGLE@24`.

## 2-3. Build and verify

Build Release, then check that `repiu_glide_render_probe` passes; that a pumpit2 run no longer
reports `_GRDRAWPOINT@4` under `GLIDE_UNIMPLEMENTED_FUNCTION` and its unimplemented counter
falls from 2 to **0**; that pumpit3 holds the Task 419 frame level of about 3,036-3,084; and
that no new decline reason appears, with `frame-errors=0` and zero GL errors. **No frame gain
is expected** — this restores missing drawing at two calls per run.

## 4. Done when

The build and probe pass, pumpit2's unimplemented counter reads zero, pumpit3 does not regress,
and the work log plus `ARCHITECTURE.md` and the Glide HLE analysis topic carry the change.
