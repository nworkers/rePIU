# 현재 실행 frontier와 다음 분석 대상

## 현재까지 도달한 상태

**확인됨:** DOS environment scan, `intro.ani`/`stage.cfg` file flow, DOS resize, arena 경계 객체 배열, allocator sentinel과 metadata store까지 진행한다. 실행 timing에 따라 생성자, allocator fault 또는 충분한 진척 뒤 quiet timeout이 먼저 관찰될 수 있다.

## 최근 해결

relocated base + `0x000F7A71`의 `8B 16` (`mov edx,[esi]`)에서 `ESI=0`인 경우를 guest `DS` zero-page read로 처리했다. 같은 명령의 고주소 source는 처리하지 않는다.

## 다음 blocker

**확인됨:** zero-page read 통과 후 relocated base + `0x000F7BAD`에서 `03 07`이 관찰된다.

```asm
add eax, dword ptr [edi]
```

관찰값 `EDI=0x026E49C4`는 기존 shadow allocator metadata 범위다.

## 다음 검증 질문

1. `03 /r` source 4바이트가 shadow memory에 모두 존재하는가?
2. 더하기 결과와 EFLAGS를 원본 x86 의미대로 복원할 수 있는가?
3. 이 연산이 allocator block size 또는 boundary 계산임을 주변 control flow로 확정할 수 있는가?
4. 반복 실행의 고주소 `ESI=0xFF000000`은 실제 sentinel encoding인지 손상된 pointer인지 구분할 수 있는가?

# Current Execution Frontier and Next Analysis Target

Execution now reaches DOS environment scanning, successful `intro.ani`/`stage.cfg` flow, DOS resize, boundary-object array initialization, and allocator sentinel/metadata stores. The `DS:0` form of `8B 16` at `0x000F7A71` has been handled without relocating low memory.

The next confirmed blocker is `03 07` at relocated offset `0x000F7BAD`, reading from `EDI=0x026E49C4` inside shadow allocator metadata. The next analysis must validate the source bytes, arithmetic flags, surrounding allocator meaning, and the separate high-address `ESI=0xFF000000` observation.
