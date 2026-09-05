# 작업 지시 20260905-602 — Linux x64 미해결 반환 주소 provenance 확정

## 목표

Task 601 이후의 host `CC/UD2`를 Linux x64 AOT 반환 resolver의 실패 경계와
연결하고, `0x010F0232`의 `RET`가 읽은 반환 주소 및 `1E7Fh` probe 경로의
의미를 확정합니다.

## 범위

1. 기존 `REPIU_LINUX_X64_RETURN_TRACE=1`과 guest watch trace를 재실행합니다.
2. `0x010F0232`의 명령 bytes와 `RET` 직전/직후 stack window를 확인합니다.
3. 실행 파일 심볼 및 disassembly로 `0x402AD3DC`/`0x402AD3DD`의 host
   소유 함수를 확인합니다.
4. far jump HLE 통과, guest `INT3` 소비, `source=0xFF` resolver 실패의
   인과관계를 `docs/analysis/linux-port-frontier.md`에 누적합니다.
5. 설계 및 작업 로그를 남깁니다.

## 코드 변경 정책

이번 작업에서는 코드 변경을 하지 않습니다. resolver가 0을 반환한 뒤
fail-closed sentinel을 우회하거나 `0xFF`를 유효한 반환 주소로 취급하는
패치는 원본 guest control flow와 미확정 private ABI를 임의로 바꾸므로
범위에서 제외합니다.

## 검증

* Linux x64 `repiu` 및 `repiu_core_probe` 빌드를 재확인합니다.
* `core_probe_all=true`를 확인합니다.
* 다음 환경으로 probe-success runtime을 재현합니다.

```text
REPIU_DOS_INT_TRACE=1
REPIU_DPMI_1E7F_TRACE=1
REPIU_DPMI_1E7F_PROBE_SUCCESS=1
REPIU_LINUX_X64_RETURN_TRACE=1
REPIU_GUEST_WATCH=0x010F0232
```

## 완료 기준

* `0x010F0232`가 `RET` 경계임을 확인합니다.
* resolver의 invalid source가 `0x000000FF`임을 확인합니다.
* host `CC/UD2`가 `RepiuLinuxX64ReturnThunk`와
  `RecoverGuestStackException`에 속함을 확인합니다.
* `1E7Fh` 실제 성공 ABI는 여전히 미확정으로 명시합니다.

---

# Work order 20260905-602 — Linux x64 unresolved return-address provenance

## Goal

Connect the post-Task-601 host `CC/UD2` to the Linux x64 AOT return-resolver
failure boundary, and establish the return address consumed by
`0x010F0232` and the meaning of the `1E7Fh` probe path.

## Scope

1. Re-run the existing `REPIU_LINUX_X64_RETURN_TRACE=1` and guest-watch trace.
2. Confirm the instruction bytes at `0x010F0232` and the stack window around
   its `RET`.
3. Use executable symbols and disassembly to identify the owners of
   `0x402AD3DC` and `0x402AD3DD`.
4. Append the causality from passing the far jump and consuming guest `INT3`
   to resolver failure with source `0xFF` in
   `docs/analysis/linux-port-frontier.md`.
5. Leave design and work-log documents.

## Code-change policy

This unit makes no code change. Bypassing the fail-closed sentinel after a zero
resolver result or treating `0xFF` as a valid return address would arbitrarily
change original guest control flow and the unresolved private ABI, so those
patches are out of scope.

## Verification

* Rebuild Linux x64 `repiu` and `repiu_core_probe`.
* Confirm `core_probe_all=true`.
* Reproduce the probe-success runtime with the environment variables shown in
  the Korean section above.

## Done criteria

* Confirm `0x010F0232` is a `RET` boundary.
* Confirm the resolver's invalid source is `0x000000FF`.
* Confirm host `CC/UD2` belongs to `RepiuLinuxX64ReturnThunk` and
  `RecoverGuestStackException`.
* Explicitly retain the unresolved status of the actual `1E7Fh` success ABI.
