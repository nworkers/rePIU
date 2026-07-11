# DOS/32A 동작 참고 명시 작업 로그

## 결과

* DOS/32A 공식 구현에서 `AX=FF00h`, `DX=0078h` 입력과 `EAX=FFFF3447h`, `GS=client data selector` 출력을 확인했다.
* DOS/32A 저장소 전체에서 PIU의 `LINEXE_LOADER` 및 관련 네 export 이름을 찾지 못했다.
* DOS/32A는 동작 교차 참고에만 사용하고 소스 코드를 복사·변형·통합하지 않는 경계를 설계 문서에 기록했다.
* Win32 HLE 코드에 공식 소스 링크, 코드 미도입 설명, 확인된 register 상수를 추가했다.
* DLL loader 분석 문서에 DOS/32A 증거와 적용 한계를 Mermaid sequence diagram과 함께 추가했다.

## 검증

`cmake --build build\win32_x86_debug --config Debug`가 성공했으며 `repiu_exe`, analyzer, loader, supervisor 및 spdlog 대상을 빌드했다. 전체 `build_win32_x86.bat` 실행은 도구 제한 시간에 도달했지만 그 전에 동일 대상의 컴파일과 링크가 완료됐고, 후속 직접 CMake 빌드가 종료 코드 0으로 확인됐다.

## 다음 단계

DOS/32A의 signature만 먼저 반환하지 않는다. PIU/DOS4GW 증거로 `GS:0x42` private module chain과 guest-callable `LINEXE_*` export를 복원한 뒤 성공 register contract를 함께 활성화한다.

# DOS/32A Behavioral Reference Attribution Work Log

Confirmed the official `AX=FF00h`, `DX=0078h` input and `EAX=FFFF3447h`, `GS=client data selector` output. No evidence of PIU's `LINEXE_LOADER` exports was found in the DOS/32A repository. The design, analysis, and source now identify DOS/32A as a behavioral cross-reference only, with no source incorporation. A direct Debug CMake build completed successfully. The signature response remains disabled until rePIU can provide the valid private environment required by PIU.
