#ifndef REPIU_HLE_BIOS_KEYBOARD_H_
#define REPIU_HLE_BIOS_KEYBOARD_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>

namespace repiu::hle
{

struct BiosKeystroke
{
    std::uint16_t legacy_ax = 0;
    std::uint16_t enhanced_ax = 0;
};

struct BiosKeyboardSnapshot
{
    std::size_t queued_count = 0;
    std::uint16_t shift_flags = 0;
    std::uint64_t accepted_count = 0;
    std::uint64_t consumed_count = 0;
    std::uint64_t overflow_count = 0;
};

class BiosKeyboard
{
public:
    static constexpr std::size_t kBufferCapacity = 15U;

    bool Push(const BiosKeystroke& keystroke);
    bool Peek(bool enhanced, std::uint16_t* ax) const;
    bool Pop(bool enhanced, std::uint16_t* ax);

    void SetShiftFlags(std::uint16_t flags);
    void UpdateShiftFlags(std::uint16_t mask, bool set);
    void ReleasePressedModifiers();
    std::uint16_t shift_flags() const;
    BiosKeyboardSnapshot Snapshot() const;

private:
    mutable std::mutex mutex_;
    std::array<BiosKeystroke, kBufferCapacity> entries_{};
    std::size_t head_ = 0;
    std::size_t size_ = 0;
    std::uint16_t shift_flags_ = 0;
    std::uint64_t accepted_count_ = 0;
    std::uint64_t consumed_count_ = 0;
    std::uint64_t overflow_count_ = 0;
};

}  // namespace repiu::hle

#endif  // REPIU_HLE_BIOS_KEYBOARD_H_
