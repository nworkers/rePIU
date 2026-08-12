# DOS 파일 생성·쓰기 서비스 작업 지시

1. `DosOpenFileHandle`에 `writable`과 출력 스트림 캐시(`DosHostWriteCache`)를
   추가합니다. 읽기 캐시와 같은 복사 규약(복사 시 cold 시작)을 따릅니다.
2. `include/repiu/hle/dos_file_system.h`에 `CreateDosFile`과 `WriteDosFile`을
   선언하고 `src/hle/dos_file_system.cpp`에 구현합니다. 생성은 부모 디렉터리만
   요구하고 기존 파일은 0바이트로 절단합니다. handle 할당은 기존 lowest-free 규칙을
   그대로 씁니다.
3. 쓰기는 파일 오프셋을 전진시키고 `cached_file_size`를 갱신합니다. 읽기 캐시는 같은
   handle에서 쓰기가 일어나면 재개 시 새 내용을 보도록 무효화합니다.
4. 생성 시 속성은 호스트 파일에 적용하지 않고 `attribute_overrides`에 기록합니다.
5. `OpenDosFile`은 access mode가 write 또는 read/write이면 handle을 쓰기 가능으로
   표시합니다. 파일 존재 요구와 읽기 경로는 그대로 둡니다.
6. `dos_int21_services.cpp`에 `case 0x3C`와 `HandleDosCreateFile`을 추가합니다.
   성공은 `CF=0, AX=handle`, 실패는 `CF=1, AX=오류코드`입니다.
7. `case 0x40`을 라우팅합니다. VFS의 열린 쓰기 가능 handle이면 파일에 쓰고, 그 외에는
   기존 콘솔 경로를 그대로 유지합니다. 쓰기를 `dos_file_io` trace에 남깁니다.
8. thread context에 생성·쓰기 계수와 마지막 생성 경로를 두고 loader 진단으로
   전달합니다.
9. `src/tools/aot_probe/dos_file_create_probe.{h,cpp}`를 추가하고 `main.cpp`와
   `CMakeLists.txt`에 등록합니다. 설계의 검증 항목 7가지를 모두 확인합니다.
10. Win32 x86 Debug 빌드를 수행하고 `aot_probe`를 실행합니다.
11. 분석 문서와 작업 로그를 갱신하고 하나의 작업 커밋으로 정리합니다.

# DOS File Create and Write Work Order

1. Add `writable` and an output stream cache (`DosHostWriteCache`) to
   `DosOpenFileHandle`, following the read cache's copy contract of starting cold.
2. Declare `CreateDosFile` and `WriteDosFile` in
   `include/repiu/hle/dos_file_system.h` and implement them in
   `src/hle/dos_file_system.cpp`. Creation requires only the parent directory and
   truncates an existing file to zero, reusing the lowest-free handle rule.
3. Make writes advance the file offset and update `cached_file_size`, and
   invalidate the read cache on the same handle so a later read sees new content.
4. Record the creation attribute in `attribute_overrides` rather than applying it
   to the host file.
5. Mark handles writable in `OpenDosFile` when the access mode is write or
   read/write, leaving the existence requirement and the read path unchanged.
6. Add `case 0x3C` and `HandleDosCreateFile` to `dos_int21_services.cpp`,
   returning `CF=0, AX=handle` on success and `CF=1, AX=error` on failure.
7. Route `case 0x40`: write to the file for an open writable VFS handle, keep
   today's console path for everything else, and trace writes in `dos_file_io`.
8. Keep create and write counters plus the last created path in the thread
   context and carry them through loader diagnostics.
9. Add `src/tools/aot_probe/dos_file_create_probe.{h,cpp}`, register it in
   `main.cpp` and `CMakeLists.txt`, and cover all seven design checks.
10. Run the Win32 x86 Debug build and execute `aot_probe`.
11. Update the analysis document and work log, and land one task commit.
