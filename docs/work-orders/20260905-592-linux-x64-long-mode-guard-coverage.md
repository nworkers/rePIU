# 작업 지시 20260905-592 — Linux x64 long-mode segment guard coverage

설계: [20260905-592](../design/20260905-592-linux-x64-long-mode-guard-coverage.md)

## 변경

Long-mode image의 guarded segment-pop을 전용 slot layout으로 coverage 검증하고,
정상·손상 synthetic image를 검증하는 portable probe를 추가합니다.

## 검증

1. `repiu_aot_probe` 실행
2. Linux x64 `repiu` 빌드
3. watched `pumpit2a` return trace 실행
4. analysis·작업 로그 갱신과 커밋

---

# Work order 20260905-592 — Linux x64 long-mode segment-guard coverage

Design: [20260905-592](../design/20260905-592-linux-x64-long-mode-guard-coverage.md)

## Change

Validate long-mode guarded segment-pop images against their dedicated slot
layout and add portable probes for valid and corrupted slots.

## Verification

1. Run `repiu_aot_probe`.
2. Build Linux x64 `repiu`.
3. Run watched `pumpit2a` return tracing.
4. Update analysis/work log and commit.
