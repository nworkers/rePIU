# Glide primitive와 면 culling / Glide primitives and face culling

## 한국어

Glide 2.4의 immediate point, line, triangle은 각각 1, 2, 3개의 `GrVertex` pointer를
받습니다. `grDrawLine`은 현재 Glide 속성이 모두 적용되는 1픽셀 폭 선입니다.

| 값 | 이름 | 의미 |
|---:|---|---|
| 0 | `GR_CULL_DISABLE` | cull 끔 |
| 1 | `GR_CULL_NEGATIVE` | 음의 signed area 제거 |
| 2 | `GR_CULL_POSITIVE` | 양의 signed area 제거 |

| origin | clockwise | counter-clockwise |
|---|---|---|
| lower-left | negative | positive |
| upper-left | positive | negative |

Cull은 triangle과 polygon에 적용되고 point와 line에는 적용되지 않습니다. OpenGL
front face를 `GL_CCW`로 유지하면 lower-left negative/positive는
`GL_BACK`/`GL_FRONT`이고 upper-left에서는 반대입니다. mode 변환에 현재 origin이
반드시 포함되어야 합니다.

원 자료:

* [3Dfx Glide Reference Manual 2.4](https://bitsavers.computerhistory.org/components/3dfx/Glide_Reference_Manual_2.4_199707.pdf)
* [3Dfx Glide Programming Guide 2.4](https://www.bitsavers.org/components/3dfx/Glide_Programming_Guide_2.4_199707.pdf)

## English

Glide 2.4 point, line, and triangle calls take one, two, and three `GrVertex`
pointers. A line is one pixel wide and uses all current attributes. Cull modes
0, 1, and 2 disable culling, reject negative signed area, and reject positive
signed area. Lower-left clockwise/counter-clockwise are negative/positive;
upper-left reverses them. Culling affects triangles and polygons, not points or
lines. An OpenGL translation must therefore use both mode and origin.
