# Privileged-trap null EIP guard design

## 목적 / Purpose

호스트 예외 처리 중 `CONTEXT::Eip == 0`일 때 privileged-instruction decoder가 null을
역참조하여 원래 실패 원인을 가리는 2차 access violation을 막습니다.

Prevent the privileged-instruction decoder from dereferencing null when
`CONTEXT::Eip == 0` during host exception handling, which otherwise masks the original
failure with a secondary access violation.

## 정책 / Policy

`HandlePrivilegedTrapInstruction`은 null context, null thread context 또는 zero EIP에서
`false`를 반환합니다. guest 상태를 수정하거나 zero EIP를 성공적으로 해석하지 않습니다.

`HandlePrivilegedTrapInstruction` returns `false` for a null context, null thread context,
or zero EIP. It neither changes guest state nor treats zero EIP as a successful decode.
