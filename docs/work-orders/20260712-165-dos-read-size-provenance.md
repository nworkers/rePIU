# DOS read size provenance 작업 지시 / Work Order

## 한국어

1. read trace에 EIP와 stack 8 dword를 bounded capture한다.
2. PIU.DAT의 마지막 nonzero read caller chain을 출력한다.
3. original object offset으로 역매핑한다.
4. 32-bit size consumer와 16-bit chunk loop 사이 truncation 지점을 확인한다.

## English

1. Bounded-capture EIP and eight stack dwords in read traces.
2. Print the caller chain for the final nonzero PIU.DAT read.
3. Map addresses to original object offsets.
4. Identify the truncation boundary and restore the evidenced generic DOS/4GW protected-mode read ABI.
