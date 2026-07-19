# 간접 원거리 LINEXE 호출 경계 작업 로그 / Indirect Far LINEXE Call Boundary Work Log

## 결과 / Result

`FF 1D disp32`를 32-bit indirect far call (`call far m16:32 ptr [disp32]`)로
인식하는 fail-closed 관측을 `HandleLinexeFarTransferBoundary`에 추가했습니다.
포인터가 guest image 안에서 읽히면 source EIP, pointer address, target offset,
selector, 그리고 확인된 LINEXE export 여부를 execution attempt로 전달해 loader
로그에 출력합니다.

이 작업은 대상의 push/RETF ABI가 확인되기 전까지 host far call, EIP skip, 또는
기존 LINEXE dispatcher 호출을 수행하지 않습니다. 따라서 원본 stack/segment 의미를
보존하고 미확정 전이는 기존 예외 경로로 남깁니다.

Added fail-closed observation for `FF 1D disp32`, decoded as a 32-bit indirect far
call (`call far m16:32 ptr [disp32]`), to `HandleLinexeFarTransferBoundary`.
When its pointer is readable inside the guest image, source EIP, pointer address,
target offset, selector, and confirmed-LINEXE-export status are propagated to the
execution attempt and printed by the loader.

Until the target's push/RETF ABI is confirmed, this task does not execute a host
far call, skip EIP, or invoke the existing LINEXE dispatcher. Unknown transfers
therefore preserve original stack/segment semantics by remaining on the existing
exception path.

## 검증 / Verification

* `cmd /c scripts\\build_win32_x86.bat`를 실행했습니다. 외부 실행 제한으로 명령은
  약 46초에 timeout 상태가 되었지만, 출력상 `repiu_exe.lib`,
  `repiu_loader_win32.exe`, `repiu_supervisor_win32.exe`가 모두 생성되어 컴파일과
  링크는 성공했습니다. 기존 코드 페이지 C4819 경고만 있었습니다.
* 새 `repiu_loader_win32.exe piu_1st` 실행에서
  `Win32 LINEXE indirect far call count/source/pointer/target` 출력이 확인됐습니다.
  이 실행은 관측 대상에 도달하지 않아 count는 0이었습니다. 따라서 대상의 실제
  selector:offset과 CALLF ABI는 다음 재현 실행에서 수집해야 합니다.

* Ran `cmd /c scripts\\build_win32_x86.bat`. The outer execution limit timed out
  after about 46 seconds, but its output shows successful compilation and linking
  of `repiu_exe.lib`, `repiu_loader_win32.exe`, and `repiu_supervisor_win32.exe`.
  Only pre-existing C4819 code-page warnings were reported.
* A new `repiu_loader_win32.exe piu_1st` run printed
  `Win32 LINEXE indirect far call count/source/pointer/target`. That run did not
  reach the observed boundary, so its count was 0. A later reproduction must
  collect the actual selector:offset and CALLF ABI.

## 다음 단계 / Next Step

`0x03042EBE` 재현 로그에서 포인터 `0x032D9C90`의 6-byte target과 예외 시 ESP를
수집합니다. target이 확인된 LINEXE export라면 CALLF가 남기는 return CS:EIP frame과
service RETF 정리 규약을 문서화한 뒤에만 dispatcher 연결을 구현합니다.

Collect the six-byte target at pointer `0x032D9C90` and exception-time ESP from a
reproduction of `0x03042EBE`. If it is a confirmed LINEXE export, document the
CALLF return CS:EIP frame and service RETF cleanup convention before implementing
dispatcher linkage.