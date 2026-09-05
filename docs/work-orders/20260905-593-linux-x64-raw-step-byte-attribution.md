# 작업 지시 20260905-593 — Linux x64 raw single-step byte 귀속

설계: [20260905-593](../design/20260905-593-linux-x64-raw-step-byte-attribution.md)

## 변경

Watched guest single-step event가 안전하게 읽은 원본 8바이트를 optional `le_bytes`로
기록하도록 확장합니다.

## 검증

1. Linux x64 `repiu` 빌드
2. `REPIU_GUEST_WATCH=0x010F010C` watched `pumpit2a`
3. 분석·작업 로그 갱신과 커밋

---

# Work order 20260905-593 — Linux x64 raw single-step byte attribution

Design: [20260905-593](../design/20260905-593-linux-x64-raw-step-byte-attribution.md)

## Change

Extend watched guest single-step events with an optional safely-read eight-byte
`le_bytes` field.

## Verification

1. Build Linux x64 `repiu`.
2. Run watched `pumpit2a` at `0x010F010C`.
3. Update analysis/work log and commit.
