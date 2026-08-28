#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace repiu::engine
{

// Emulates a 93C46 Microwire Serial EEPROM (1Kbit, 64 x 16-bit organization).
class Eeprom93c46
{
public:
    explicit Eeprom93c46(const std::string& filepath);
    ~Eeprom93c46();

    // Reset/Load from disk
    void Load();

    // Persist to disk
    void Save();

    // Handle Write on control port (e.g. 0x02AC)
    // Extracts CS, CLK, DI from the provided value.
    // cs = bit 0, clk = bit 1, di = bit 2
    void WriteControl(std::uint8_t value);

    // Read from data port (e.g. 0x02AE)
    // Returns 1 if DO is high, 0 if low.
    std::uint8_t ReadData() const;

private:
    void ProcessBit(bool cs, bool clk, bool di);
    void ExecuteCommand();

    std::string filepath_;
    std::vector<std::uint16_t> memory_;

    enum class State
    {
        Standby,
        WaitStart,
        WaitOpcode,
        WaitAddress,
        WaitData,
        ReadData
    };

    State state_;

    bool cs_;
    bool clk_;
    bool di_;
    bool do_;
    
    bool write_enabled_;

    std::uint8_t opcode_;
    std::uint8_t address_;
    std::uint16_t data_shift_reg_;
    int bit_count_;
};

} // namespace repiu::engine
