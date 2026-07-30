# 작업 로그: Glide 선 그리기와 양방향 cull / Work log: Glide line drawing and bidirectional culling

Task 374. 설계: [20260731-374](../design/20260731-374-glide-line-and-cull.md)
작업 지시: [20260731-374](../work-orders/20260731-374-glide-line-and-cull.md)

## 한국어

### 결과

`gameplay-capture.log`에서 확인한 `grDrawLine` 미구현 11,024건과
`grCullMode(1)` 미지원 1건을 구현했습니다. 원본 실행 파일과 게임 로직은 변경하지
않았습니다.

* 60바이트 producer vertex 해석을 플랫폼 공용 HLE decoder로 분리하고 기존 triangle과
  새 line 경로가 함께 사용하도록 했습니다.
* line boundary는 두 guest 범위를 각각 검증하고 복사본만 decode한 뒤 기존 `ret 8`
  계약으로 복귀합니다.
* OpenGL backend는 line과 triangle의 텍스처, 색, reciprocal-w, fog 제출 경로를 공유하며
  line은 폭 1의 `GL_LINES`로 제출합니다.
* cull mode 0/1/2를 현재 Glide origin까지 고려해 OpenGL disable/front/back으로
  변환합니다. 범위 밖 mode의 safe decline은 유지했습니다.
* 설계, 아키텍처, 누적 분석, Glide primitive/culling KB와 색인을 갱신했습니다.

```mermaid
flowchart LR
    E[캡처 증거] --> D[공용 vertex decode]
    D --> L[GL_LINES 제출]
    E --> C[origin-aware cull 변환]
    L --> P[결정적 probe]
    C --> P
    P --> B[Debug와 Release 빌드]
```

### 검증

| 검증 | 결과 |
|---|---|
| Win32 x86 Debug 전체 빌드 | 통과 |
| Win32 x86 Release 전체 빌드 | 통과 |
| Debug/Release `repiu_glide_render_probe` | 모두 통과 |
| Debug/Release `repiu_glide_issue_probe` | 모두 통과; 의도된 synthetic issue 기록 뒤 pass |
| vertex 정상값과 짧은 입력 거부 probe | 통과 |
| origin 2종 × cull mode 3종과 잘못된 mode probe | 통과 |
| `git diff --check` | 오류 없음; 저장소의 CRLF 변환 예고만 출력 |

제공된 gameplay 장면은 SERVICE/TEST 입력을 거치는 대화형 장면이므로 이번 자동 검증에서
같은 장면을 재실행하지 않았습니다. 후속 실기동 성공 기준은 해당 장면의 issue summary에서
이번 두 호출에 대한 `unimplemented=0`, `unsupported=0`을 확인하는 것입니다. 캡처에서
호출되지 않은 point, polygon, AA primitive는 별도 범위로 남습니다.

## English

Implemented the 11,024 unimplemented `grDrawLine` calls and the single rejected
`grCullMode(1)` observed in `gameplay-capture.log`, without changing the original
executable or gameplay logic. A shared HLE decoder now owns the observed
60-byte producer vertex ABI. Both triangle and line boundaries use it after
guest-range validation, and line keeps the existing `ret 8` contract.

The OpenGL backend shares attribute submission between lines and triangles,
submits one-pixel `GL_LINES`, and translates cull modes 0 through 2 using the
active Glide origin. Invalid modes retain the safe-decline behavior.

Full Win32 x86 Debug and Release builds passed. Debug and Release render and
issue probes passed, including deterministic vertex decoding, short-input
rejection, every origin/cull mapping, and invalid-mode rejection. `git diff
--check` reported no errors.

The input-dependent SERVICE/TEST gameplay scene was not replayed automatically.
A later live replay should confirm zero unimplemented or unsupported issues for
these two calls. Point, polygon, and antialiased primitives were not observed in
the capture and remain separate work.
