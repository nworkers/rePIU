# LINEXE module name 관찰 작업 로그

selector `0090h` descriptor는 base `0x035D5000`으로 유효하고 module name pointer는 `0090:0504`, direct string은 `LINEXE_LOADER`로 정확했습니다. 그러나 GS byte-load는 0회여서 이름 instruction 전 단계에서 중단됨을 확인했습니다.

# LINEXE Module-Name Observation Work Log

Confirmed selector `0090h`, pointer `0090:0504`, and direct `LINEXE_LOADER` bytes. Zero GS byte loads prove failure occurs before name comparison.
