# Shadow allocator link provenance 작업 지시

```mermaid
flowchart LR
    D["Design"] --> P["Shadow writer provenance"]
    P --> R["Allocator read correlation"]
    R --> O["Loader output"]
    O --> A["Repeated analysis"]
    A --> V["Regression + documentation"]
```

## 작업

1. internal shadow writer provenance 구조와 byte-address map을 추가한다.
2. 모든 기존 shadow write에 writer metadata를 연결한다.
3. allocator metadata read 결과와 complete writer를 control-flow ring에 첨부한다.
4. loader에서 read와 writer 관계를 출력한다.
5. 반복 실행으로 null/`0xFF000000` link writer를 식별한다.
6. 전체 test, 문서 갱신과 커밋을 수행한다.

# Shadow Allocator Link Provenance Work Order

Add internal per-byte shadow writer provenance, correlate successful allocator metadata reads with a complete four-byte writer, expose the relationship through the existing control-flow ring and loader, identify null or `0xFF000000` link writers in repeated execution, run full tests, update documentation, and commit the task.
