#include "eeprom_93c46.h"
#include <fstream>

namespace repiu::engine
{

Eeprom93c46::Eeprom93c46(const std::string& filepath)
    : filepath_(filepath),
      memory_(64, 0xFFFF),
      state_(State::Standby),
      cs_(false), clk_(false), di_(false), do_(true),
      write_enabled_(false),
      opcode_(0), address_(0), data_shift_reg_(0), bit_count_(0)
{
    Load();
}

Eeprom93c46::~Eeprom93c46()
{
    Save();
}

void Eeprom93c46::Load()
{
    std::ifstream file(filepath_, std::ios::binary);
    if (file.is_open())
    {
        file.read(reinterpret_cast<char*>(memory_.data()), memory_.size() * sizeof(std::uint16_t));
    }
    else
    {
        for (auto& word : memory_)
        {
            word = 0xFFFF;
        }
        Save();
    }
}

void Eeprom93c46::Save()
{
    std::ofstream file(filepath_, std::ios::binary | std::ios::trunc);
    if (file.is_open())
    {
        file.write(reinterpret_cast<const char*>(memory_.data()), memory_.size() * sizeof(std::uint16_t));
    }
}

void Eeprom93c46::WriteControl(std::uint8_t value)
{
    bool new_cs = (value & 0x01) != 0;
    bool new_clk = (value & 0x02) != 0;
    bool new_di = (value & 0x04) != 0;

    if (cs_ && !new_cs)
    {
        state_ = State::Standby;
        do_ = true;
    }
    else if (!cs_ && new_cs)
    {
        state_ = State::WaitStart;
        do_ = true;
    }

    if (new_cs && !clk_ && new_clk)
    {
        ProcessBit(new_cs, new_clk, new_di);
    }

    cs_ = new_cs;
    clk_ = new_clk;
    di_ = new_di;
}

std::uint8_t Eeprom93c46::ReadData() const
{
    return do_ ? 1 : 0;
}

void Eeprom93c46::ProcessBit(bool cs, bool clk, bool di)
{
    switch (state_)
    {
        case State::Standby:
            break;

        case State::WaitStart:
            if (di)
            {
                state_ = State::WaitOpcode;
                bit_count_ = 0;
                opcode_ = 0;
            }
            break;

        case State::WaitOpcode:
            opcode_ = static_cast<std::uint8_t>((opcode_ << 1) | (di ? 1 : 0));
            bit_count_++;
            if (bit_count_ == 2)
            {
                state_ = State::WaitAddress;
                bit_count_ = 0;
                address_ = 0;
            }
            break;

        case State::WaitAddress:
            address_ = static_cast<std::uint8_t>((address_ << 1) | (di ? 1 : 0));
            bit_count_++;
            if (bit_count_ == 6)
            {
                if (opcode_ == 0x01) // WRITE
                {
                    state_ = State::WaitData;
                    bit_count_ = 0;
                    data_shift_reg_ = 0;
                }
                else if (opcode_ == 0x02) // READ
                {
                    state_ = State::ReadData;
                    bit_count_ = 0;
                    do_ = false; // Dummy 0 bit before data
                    data_shift_reg_ = memory_[address_ & 0x3F];
                }
                else if (opcode_ == 0x00 || opcode_ == 0x03) // EXTENDED or ERASE
                {
                    ExecuteCommand();
                    state_ = State::Standby;
                }
            }
            break;

        case State::WaitData:
            data_shift_reg_ = static_cast<std::uint16_t>((data_shift_reg_ << 1) | (di ? 1 : 0));
            bit_count_++;
            if (bit_count_ == 16)
            {
                ExecuteCommand();
                state_ = State::Standby;
            }
            break;

        case State::ReadData:
            bit_count_++;
            if (bit_count_ <= 16)
            {
                do_ = (data_shift_reg_ & 0x8000) != 0;
                data_shift_reg_ <<= 1;
            }
            else
            {
                do_ = true;
            }
            break;
    }
}

void Eeprom93c46::ExecuteCommand()
{
    if (opcode_ == 0x00)
    {
        std::uint8_t ext_op = (address_ >> 4) & 0x03;
        if (ext_op == 0x00) // EWDS
        {
            write_enabled_ = false;
        }
        else if (ext_op == 0x03) // EWEN
        {
            write_enabled_ = true;
        }
        else if (ext_op == 0x02) // ERAL
        {
            if (write_enabled_)
            {
                for (auto& word : memory_) word = 0xFFFF;
                Save();
            }
        }
    }
    else if (opcode_ == 0x01) // WRITE
    {
        if (write_enabled_)
        {
            memory_[address_ & 0x3F] = data_shift_reg_;
            Save();
        }
    }
    else if (opcode_ == 0x03) // ERASE
    {
        if (write_enabled_)
        {
            memory_[address_ & 0x3F] = 0xFFFF;
            Save();
        }
    }
}

} // namespace repiu::engine
