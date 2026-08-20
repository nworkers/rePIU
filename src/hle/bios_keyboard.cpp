#include "repiu/hle/bios_keyboard.h"

namespace repiu::hle
{

bool BiosKeyboard::Push(const BiosKeystroke& keystroke)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (size_ == entries_.size())
    {
        ++overflow_count_;
        return false;
    }
    entries_[(head_ + size_) % entries_.size()] = keystroke;
    ++size_;
    ++accepted_count_;
    return true;
}

bool BiosKeyboard::Peek(bool enhanced, std::uint16_t* ax) const
{
    if (ax == nullptr)
    {
        return false;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (size_ == 0U)
    {
        return false;
    }
    *ax = enhanced ? entries_[head_].enhanced_ax : entries_[head_].legacy_ax;
    return true;
}

bool BiosKeyboard::Pop(bool enhanced, std::uint16_t* ax)
{
    if (ax == nullptr)
    {
        return false;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (size_ == 0U)
    {
        return false;
    }
    *ax = enhanced ? entries_[head_].enhanced_ax : entries_[head_].legacy_ax;
    head_ = (head_ + 1U) % entries_.size();
    --size_;
    ++consumed_count_;
    return true;
}

void BiosKeyboard::SetShiftFlags(std::uint16_t flags)
{
    std::lock_guard<std::mutex> lock(mutex_);
    shift_flags_ = flags;
}

void BiosKeyboard::UpdateShiftFlags(std::uint16_t mask, bool set)
{
    std::lock_guard<std::mutex> lock(mutex_);
    shift_flags_ = set
        ? static_cast<std::uint16_t>(shift_flags_ | mask)
        : static_cast<std::uint16_t>(shift_flags_ & ~mask);
}

void BiosKeyboard::ReleasePressedModifiers()
{
    std::lock_guard<std::mutex> lock(mutex_);
    shift_flags_ &= 0x00F0U;
}

std::uint16_t BiosKeyboard::shift_flags() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return shift_flags_;
}

BiosKeyboardSnapshot BiosKeyboard::Snapshot() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    BiosKeyboardSnapshot snapshot;
    snapshot.queued_count = size_;
    snapshot.shift_flags = shift_flags_;
    snapshot.accepted_count = accepted_count_;
    snapshot.consumed_count = consumed_count_;
    snapshot.overflow_count = overflow_count_;
    return snapshot;
}

}  // namespace repiu::hle
