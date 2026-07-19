# AOT Probe 문자열 검색 설계

## 목적

`repiu_aot_probe`에 재배치된 런타임 이미지 전체에서 지정한 ASCII 바이트열을 찾는 읽기 전용 진단 모드를 추가합니다. 이 기능은 원본 실행 파일이나 런타임 상태를 변경하지 않고, 오류 문구·심볼 흔적·리소스 식별자와 코드/데이터 주소의 연관성을 조사하는 데 사용합니다.

## 동작

```mermaid
flowchart LR
    A[PIU.EXE 로드 및 재배치] --> B[--findstr 문자열 수신]
    B --> C[모든 재배치 객체의 바이트 검사]
    C --> D[일치한 런타임 가상 주소 출력]
```

`--findstr <text>`는 각 재배치 객체의 바이트를 순차적으로 비교하고 일치 시작 주소를 `findstr=0x...` 형식으로 출력합니다. 빈 문자열은 검색하지 않습니다. 주소는 AOT 프로브의 다른 출력과 동일하게 재배치 후 런타임 가상 주소를 사용합니다.

## 범위와 제약

이 검색은 ASCII/단일 바이트 리터럴 검색이며 인코딩 변환, 와일드카드, 역참조 추적을 수행하지 않습니다. 결과는 문자열을 참조하는 코드 위치가 아니라 문자열 리터럴이 있는 위치입니다.

---

# AOT Probe String-Search Design

## Purpose

Add a read-only diagnostic mode to `repiu_aot_probe` that searches the complete relocated runtime image for a supplied ASCII byte sequence. It does not change the original executable or runtime state, and supports investigation of error text, symbol remnants, resource identifiers, and their relation to code/data addresses.

## Behavior

`--findstr <text>` compares bytes through every relocated object and prints each matching start address as `findstr=0x...`. Empty strings are not searched. Addresses use relocated runtime virtual addresses, consistent with the probe's other output.

## Scope and limits

The mode is an ASCII/single-byte literal search. It performs no encoding conversion, wildcard matching, or reference tracing. A result identifies the string literal location, not code that references it.
