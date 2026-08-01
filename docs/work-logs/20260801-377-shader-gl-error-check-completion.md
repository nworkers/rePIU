# ?? ??: GLSL shader `glGetError` ?? ?? / Work log: complete the GLSL shader `glGetError` policy

Task 377. ??: [20260801-377](../design/20260801-377-shader-gl-error-check-completion.md). ?? ??: [20260801-377](../work-orders/20260801-377-shader-gl-error-check-completion.md).

## ???

* `GlideOpenGlShader`? fog ? ?? combine ? ?? ?? GL error ??? ????, ??? ?? include ??? ??????.
* ?? ???? ?? `glGetError()`? ?? ?? ??? ?? ?? ?? ?? ?? ?? ??? ???? `git diff --check`? ??????.
* Win32 Release ??/`repiu_exe` ?? ??? ?? ?????, ?? ???? ?? ????? ?? 120? ?? ??? ??????. ??? ??? ???? ???? ???? ???? ????.
* ?? gameplay A/B? `REPIU_GLIDE_SWAP_INTERVAL=0`, `REPIU_GLIDE_ORDINAL_TIME_PROFILE=1` ???? ?? FPS ?? ???? ?????.

## English

* Connected the three fog setters and two combine setters in `GlideOpenGlShader` to the existing GL-error policy, and repaired the discovered leading-include typo.
* Static inspection confirmed each of the five `glGetError()` calls is now behind a policy short circuit or guarded error branch; `git diff --check` passed.
* Both the full Win32 Release build and the `repiu_exe` target were attempted, but each triggered a broad project rebuild and hit the 120-second execution limit. No compiler error was printed, but this is not claimed as a successful build.
* The follow-up gameplay A/B uses `REPIU_GLIDE_SWAP_INTERVAL=0` and `REPIU_GLIDE_ORDINAL_TIME_PROFILE=1` on the actual FPS-collapse scene.

## Gameplay ?? ?? / Gameplay measurement

2026-08-01 ?? FPS ?? ??? 32.766? capture? 1,246 frame(? 38.0 FPS)?????. shader ??? per-call check OFF, frame check 0?? `GL_KHR_debug` callback? ??? 2???? 0?? ??????.
Glide gate? wall 11.75%?? VEH 21.82% ? unaccounted 78.18%?? ?????. ??? shader ?? ?? ??? ?? ????? ? ??? ?? ?? ?? Glide? ??? guest/exception ?? ?????.

## English

The 32.766-second capture of the real FPS-collapse scene produced 1,246 frames (about 38.0 FPS). The shader policy had per-call checks OFF and zero frame checks; the `GL_KHR_debug` callback reported two messages and zero errors.
The Glide gate was 11.75% of wall, below VEH at 21.82% and unaccounted at 78.18%. The shader error-check removal is therefore verified as clean, but the next performance axis in this scene is guest/exception execution boundaries rather than Glide.

## 후속 조사 인계 / Follow-up investigation handoff

Task 377의 shader 정책 완료 후 Task 378~392에서 이어진 실행 경계 최적화 결과는 [20260802-393-performance-investigation-handoff.md](../design/20260802-393-performance-investigation-handoff.md)에 종합했습니다.

The execution-boundary work continued through Tasks 378-392 after the Task 377 shader-policy completion; its consolidated handoff is [20260802-393-performance-investigation-handoff.md](../design/20260802-393-performance-investigation-handoff.md).