# 실제 contiguous runtime arena 확장 작업 지시

1. PIU runtime arena expansion을 16 MiB로 늘린다.
2. loader 출력과 regression 기대값을 갱신한다.
3. Win32 x86 빌드와 hello sample을 검증한다.
4. supervisor로 PIU allocator boundary와 다음 frontier를 확인한다.
5. 결과를 문서화하고 커밋한다.

# Contiguous Runtime Arena Expansion Work Order

Increase PIU runtime arena expansion to 16 MiB, update loader/regression expectations, validate the Win32 x86 build and hello sample, use the supervisor to verify the allocator boundary and next frontier, document results, and commit.
