#include "repiu/sound/ymz280b_device.h"

#include <algorithm>
#include <cstring>

namespace repiu::sound
{
namespace
{

// Ported from MAME src/devices/sound/ymz280b.cpp (BSD-3-Clause, Aaron Giles).
// diff_lookup[nib] = (nib & 8 ? -1 : 1) * (2 * (nib & 7) + 1)
constexpr int kDiffLookup[16] = {
    1,  3,  5,  7,  9,  11,  13,  15,
    -1, -3, -5, -7, -9, -11, -13, -15,
};

// Step size scaling per nibble magnitude, in 1/256 units.
constexpr int kIndexScale[8] = {
    0x0E6, 0x0E6, 0x0E6, 0x0E6, 0x133, 0x199, 0x200, 0x266,
};

// Resampling fraction. FRAC_ONE is the advance that consumes exactly one source
// sample per output sample, so a voice's source rate is
// output_rate * output_step / kFracOne.
constexpr unsigned kFracBits = 9;
constexpr std::int32_t kFracOne = 1 << kFracBits;

// Upper bound on source samples decoded for one Generate() call, matching MAME's
// MAX_SAMPLE_CHUNK. Two extra scratch slots absorb the single lookahead sample
// the resampler consumes past the last emitted frame.
constexpr std::uint32_t kMaxSampleChunk = 10000U;
constexpr std::uint32_t kScratchSlack = 2U;

// Bound on the key-on report queue. Only logging consumes it, so dropping the
// oldest entries is preferable to unbounded growth if nobody drains it.
constexpr std::size_t kMaxPendingKeyOnEvents = 64U;

// MAME accumulates interp * volume / 2 against a 32768 * 256 normalization, so
// converting the accumulator back to s16 is a divide by 256.
constexpr std::int32_t kMixNormalizeDivisor = 256;

}  // namespace

void Ymz280bDevice::Initialize(std::vector<std::uint8_t> rom,
                               std::uint32_t master_clock_hz)
{
    rom_ = std::move(rom);
    master_clock_hz_ = master_clock_hz;

    // MAME: m_master_clock = clock / 384, INTERNAL_SAMPLE_RATE = m_master_clock * 2.
    output_sample_rate_ = master_clock_hz_ / 384U * 2U;

    scratch_.assign(kMaxSampleChunk + kScratchSlack, 0);
    Reset();
}

void Ymz280bDevice::Reset()
{
    // MAME clears every register from 0xff down to 0x00 through the normal write
    // path so that side effects (key-off, volume recompute) run as on hardware.
    for (int reg = 0xFF; reg >= 0; --reg)
    {
        current_register_ = static_cast<std::uint8_t>(reg);
        WriteToRegister(0);
    }

    current_register_ = 0;
    status_register_ = 0;
    external_memory_address_ = 0;

    for (Voice& voice : voices_)
    {
        voice.curr_sample = 0;
        voice.last_sample = 0;
        voice.output_pos = kFracOne;
        voice.playing = false;
    }

    key_on_events_.clear();
    register_write_count_ = 0;
    key_on_count_ = 0;
}

std::uint8_t Ymz280bDevice::ReadByte(std::uint32_t byte_address) const
{
    // The chip drives a 24-bit external address bus. Unpopulated space floats
    // high, so anything past the dumped ROM reads 0xFF.
    const std::uint32_t masked = byte_address & 0xFFFFFFU;
    if (masked >= rom_.size())
    {
        return 0xFFU;
    }
    return rom_[masked];
}

void Ymz280bDevice::UpdateStep(Voice* voice)
{
    const int frequency =
        voice->mode == kModeAdpcm ? (voice->fnum & 0x0FF) : (voice->fnum & 0x1FF);
    voice->output_step = static_cast<std::uint32_t>(frequency + 1);
}

void Ymz280bDevice::UpdateVolumes(Voice* voice)
{
    if (voice->pan == 8)
    {
        voice->output_left = voice->level;
        voice->output_right = voice->level;
    }
    else if (voice->pan < 8)
    {
        voice->output_left = voice->level;
        voice->output_right =
            (voice->pan == 0) ? 0 : voice->level * (voice->pan - 1) / 7;
    }
    else
    {
        voice->output_left = voice->level * (15 - voice->pan) / 7;
        voice->output_right = voice->level;
    }
}

void Ymz280bDevice::RecordKeyOn(int index, const Voice& voice)
{
    ++key_on_count_;
    if (key_on_events_.size() >= kMaxPendingKeyOnEvents)
    {
        key_on_events_.erase(key_on_events_.begin());
    }
    KeyOnEvent event;
    event.voice = index;
    event.mode = voice.mode;
    event.start_byte_address = voice.start / 2U;
    event.stop_byte_address = voice.stop / 2U;
    event.sample_rate_hz = static_cast<std::uint32_t>(
        static_cast<std::uint64_t>(output_sample_rate_) * voice.output_step /
        static_cast<std::uint32_t>(kFracOne));
    event.level = voice.level;
    event.pan = voice.pan;
    event.looping = voice.looping;
    key_on_events_.push_back(event);
}

std::vector<Ymz280bDevice::KeyOnEvent> Ymz280bDevice::TakeKeyOnEvents()
{
    std::vector<KeyOnEvent> events;
    events.swap(key_on_events_);
    return events;
}

void Ymz280bDevice::WriteRegisterSelect(std::uint8_t value)
{
    current_register_ = value;
}

void Ymz280bDevice::WriteRegisterData(std::uint8_t value)
{
    ++register_write_count_;
    WriteToRegister(value);
}

std::uint8_t Ymz280bDevice::ReadExternalMemory()
{
    if (!external_memory_enable_)
    {
        return 0xFFU;
    }
    const std::uint8_t value = external_read_latch_;
    external_read_latch_ = ReadByte(external_memory_address_);
    external_memory_address_ = (external_memory_address_ + 1U) & 0xFFFFFFU;
    return value;
}

std::uint8_t Ymz280bDevice::ReadStatus()
{
    const std::uint8_t value = status_register_;
    status_register_ = 0;
    return value;
}

bool Ymz280bDevice::AnyVoicePlaying() const
{
    for (const Voice& voice : voices_)
    {
        if (voice.playing)
        {
            return true;
        }
    }
    return false;
}

void Ymz280bDevice::WriteToRegister(std::uint8_t data)
{
    if (current_register_ < 0x80)
    {
        const int index = (current_register_ >> 2) & 7;
        Voice* voice = &voices_[index];

        switch (current_register_ & 0xE3)
        {
            case 0x00:  // pitch, low 8 bits
                voice->fnum = (voice->fnum & 0x100) | (data & 0xFF);
                UpdateStep(voice);
                break;

            case 0x01:  // pitch upper bit, loop, key on, mode
            {
                voice->fnum = (voice->fnum & 0xFF) | ((data & 0x01) << 8);
                voice->looping = (data & 0x10) != 0;
                std::uint8_t effective = data;
                if ((data & 0x60) == 0)
                {
                    // Mode 0 does not exist; the chip behaves as if key-off.
                    effective = static_cast<std::uint8_t>(data & 0x7F);
                }
                else
                {
                    voice->mode = (data & 0x60) >> 5;
                }
                // Both fnum and mode are final here, so deriving the step now
                // rather than after the key-on check (as MAME does) is
                // equivalent, and it lets the key-on report state the real rate.
                UpdateStep(voice);

                const bool key_on_requested = (effective & 0x80) != 0;
                if (!voice->keyon && key_on_requested && keyon_enable_)
                {
                    voice->playing = true;
                    voice->position = voice->start;
                    voice->signal = 0;
                    voice->loop_signal = 0;
                    voice->step = 0x7F;
                    voice->loop_step = 0x7F;
                    voice->loop_count = 0;
                    RecordKeyOn(index, *voice);
                }
                else if (voice->keyon && !key_on_requested)
                {
                    voice->playing = false;
                }
                voice->keyon = key_on_requested;
                break;
            }

            case 0x02:  // total level
                voice->level = data;
                UpdateVolumes(voice);
                break;

            case 0x03:  // panpot
                voice->pan = data & 0x0F;
                UpdateVolumes(voice);
                break;

            // Addresses are held in nibble units, so the register bytes compose
            // as (high << 17) | (mid << 9) | (low << 1).
            case 0x20:
                voice->start = (voice->start & (0x00FFFF << 1)) | (data << 17);
                break;
            case 0x21:
                voice->loop_start =
                    (voice->loop_start & (0x00FFFF << 1)) | (data << 17);
                break;
            case 0x22:
                voice->loop_end =
                    (voice->loop_end & (0x00FFFF << 1)) | (data << 17);
                break;
            case 0x23:
                voice->stop = (voice->stop & (0x00FFFF << 1)) | (data << 17);
                break;

            case 0x40:
                voice->start = (voice->start & (0xFF00FF << 1)) | (data << 9);
                break;
            case 0x41:
                voice->loop_start =
                    (voice->loop_start & (0xFF00FF << 1)) | (data << 9);
                break;
            case 0x42:
                voice->loop_end =
                    (voice->loop_end & (0xFF00FF << 1)) | (data << 9);
                break;
            case 0x43:
                voice->stop = (voice->stop & (0xFF00FF << 1)) | (data << 9);
                break;

            case 0x60:
                voice->start = (voice->start & (0xFFFF00 << 1)) | (data << 1);
                break;
            case 0x61:
                voice->loop_start =
                    (voice->loop_start & (0xFFFF00 << 1)) | (data << 1);
                break;
            case 0x62:
                voice->loop_end =
                    (voice->loop_end & (0xFFFF00 << 1)) | (data << 1);
                break;
            case 0x63:
                voice->stop = (voice->stop & (0xFFFF00 << 1)) | (data << 1);
                break;

            default:
                break;
        }
        return;
    }

    switch (current_register_)
    {
        // 0x80-0x82 are the DSP registers, which MAME does not implement either.
        case 0x80:
        case 0x81:
        case 0x82:
            break;

        case 0x84:  // ROM readback address, high
            external_memory_address_high_ = static_cast<std::uint32_t>(data) << 16;
            break;

        case 0x85:  // ROM readback address, middle
            external_memory_address_mid_ = static_cast<std::uint32_t>(data) << 8;
            break;

        case 0x86:  // ROM readback address, low; refreshes the latch
            external_memory_address_ = external_memory_address_high_ |
                                       external_memory_address_mid_ | data;
            if (external_memory_enable_)
            {
                external_read_latch_ = ReadByte(external_memory_address_);
            }
            break;

        case 0x87:  // external RAM write, ignored in a ROM-only configuration
            if (external_memory_enable_)
            {
                external_memory_address_ =
                    (external_memory_address_ + 1U) & 0xFFFFFFU;
            }
            break;

        case 0xFE:  // IRQ mask
            irq_mask_ = data;
            break;

        case 0xFF:  // key-on enable, IRQ enable, external memory enable
            external_memory_enable_ = (data & 0x40) != 0;
            irq_enable_ = (data & 0x10) != 0;

            if (keyon_enable_ && (data & 0x80) == 0)
            {
                for (Voice& voice : voices_)
                {
                    voice.playing = false;
                }
            }
            else if (!keyon_enable_ && (data & 0x80) != 0)
            {
                for (int index = 0; index < kVoiceCount; ++index)
                {
                    Voice& voice = voices_[index];
                    if (voice.keyon && voice.looping)
                    {
                        voice.playing = true;
                        RecordKeyOn(index, voice);
                    }
                }
            }
            keyon_enable_ = (data & 0x80) != 0;
            break;

        default:
            break;
    }
}

std::uint32_t Ymz280bDevice::GenerateAdpcm(Voice* voice, std::int16_t* buffer,
                                           std::uint32_t samples)
{
    std::uint32_t position = voice->position;
    int signal = voice->signal;
    int step = voice->step;

    const auto decode_one = [&]() {
        const int val =
            ReadByte(position / 2U) >> ((~position & 1U) << 2);
        signal += (step * kDiffLookup[val & 15]) / 8;
        signal = std::clamp(signal, -32768, 32767);
        step = (step * kIndexScale[val & 7]) >> 8;
        step = std::clamp(step, 0x7F, 0x6000);
        *buffer++ = static_cast<std::int16_t>(signal);
    };

    if (!voice->looping)
    {
        while (samples != 0)
        {
            decode_one();
            --samples;
            ++position;
            if (position >= voice->stop)
            {
                voice->ended = true;
                break;
            }
        }
    }
    else
    {
        while (samples != 0)
        {
            decode_one();
            --samples;
            ++position;
            // The ADPCM predictor is history dependent, so the state at the loop
            // point has to be captured on the first pass and restored on every
            // wrap. Re-decoding from a reset predictor would click.
            if (position == voice->loop_start && voice->loop_count == 0)
            {
                voice->loop_signal = signal;
                voice->loop_step = step;
            }
            if (position >= voice->loop_end && voice->keyon)
            {
                position = voice->loop_start;
                signal = voice->loop_signal;
                step = voice->loop_step;
                ++voice->loop_count;
            }
            if (position >= voice->stop)
            {
                voice->ended = true;
                break;
            }
        }
    }

    voice->position = position;
    voice->signal = signal;
    voice->step = step;
    return samples;
}

std::uint32_t Ymz280bDevice::GeneratePcm8(Voice* voice, std::int16_t* buffer,
                                          std::uint32_t samples)
{
    std::uint32_t position = voice->position;

    while (samples != 0)
    {
        const auto val = static_cast<std::int8_t>(ReadByte(position / 2U));
        *buffer++ = static_cast<std::int16_t>(val * 256);
        --samples;
        position += 2U;
        if (voice->looping && position >= voice->loop_end && voice->keyon)
        {
            position = voice->loop_start;
        }
        if (position >= voice->stop)
        {
            voice->ended = true;
            break;
        }
    }

    voice->position = position;
    return samples;
}

std::uint32_t Ymz280bDevice::GeneratePcm16(Voice* voice, std::int16_t* buffer,
                                           std::uint32_t samples)
{
    std::uint32_t position = voice->position;

    while (samples != 0)
    {
        const auto low = static_cast<std::uint16_t>(ReadByte(position / 2U));
        const auto high = static_cast<std::uint16_t>(ReadByte(position / 2U + 1U));
        *buffer++ =
            static_cast<std::int16_t>(static_cast<std::uint16_t>(high << 8) | low);
        --samples;
        position += 4U;
        if (voice->looping && position >= voice->loop_end && voice->keyon)
        {
            position = voice->loop_start;
        }
        if (position >= voice->stop)
        {
            voice->ended = true;
            break;
        }
    }

    voice->position = position;
    return samples;
}

void Ymz280bDevice::Generate(std::int16_t* output, std::uint32_t frames)
{
    if (output == nullptr || frames == 0)
    {
        return;
    }

    const std::size_t sample_count = static_cast<std::size_t>(frames) * 2U;
    accumulator_.assign(sample_count, 0);

    if (!rom_.empty())
    {
        for (int index = 0; index < kVoiceCount; ++index)
        {
            Voice* voice = &voices_[index];
            std::int16_t prev = voice->last_sample;
            std::int16_t curr = voice->curr_sample;
            std::uint32_t scratch_index = 0;
            std::uint32_t sample_index = 0;
            std::uint32_t remaining = frames;
            const int left_volume = voice->output_left;
            const int right_volume = voice->output_right;

            // Silent and stopped: nothing to mix, and arming output_pos makes the
            // next key-on start immediately instead of after a fractional delay.
            if (!voice->playing && curr == 0 && prev == 0)
            {
                voice->output_pos = kFracOne;
                continue;
            }

            const auto emit = [&](std::int16_t sample_prev,
                                  std::int16_t sample_curr) {
                const std::int32_t interpolated =
                    ((static_cast<std::int32_t>(sample_prev) *
                      (kFracOne - voice->output_pos)) +
                     (static_cast<std::int32_t>(sample_curr) *
                      voice->output_pos)) >>
                    kFracBits;
                accumulator_[static_cast<std::size_t>(sample_index) * 2U] +=
                    interpolated * left_volume / 2;
                accumulator_[static_cast<std::size_t>(sample_index) * 2U + 1U] +=
                    interpolated * right_volume / 2;
                ++sample_index;
                voice->output_pos +=
                    static_cast<std::int32_t>(voice->output_step);
                --remaining;
            };

            // Finish the sample that was in flight when the previous block ended.
            while (remaining > 0 && voice->output_pos < kFracOne)
            {
                emit(prev, curr);
            }
            if (voice->output_pos >= kFracOne)
            {
                voice->output_pos -= kFracOne;
            }
            else
            {
                continue;
            }

            const std::int32_t final_pos =
                voice->output_pos +
                static_cast<std::int32_t>(remaining * voice->output_step);
            std::uint32_t new_samples =
                static_cast<std::uint32_t>((final_pos + kFracOne) >> kFracBits);
            new_samples = std::min(new_samples, kMaxSampleChunk);
            std::uint32_t samples_left = new_samples;

            // Keep the lookahead slots deterministic; the resampler may read one
            // sample past the last one the generators filled.
            std::fill_n(scratch_.begin() + new_samples, kScratchSlack, 0);

            switch ((voice->playing ? 0x80 : 0x00) | voice->mode)
            {
                case 0x80 | kModeAdpcm:
                    samples_left =
                        GenerateAdpcm(voice, scratch_.data(), new_samples);
                    break;
                case 0x80 | kModePcm8:
                    samples_left =
                        GeneratePcm8(voice, scratch_.data(), new_samples);
                    break;
                case 0x80 | kModePcm16:
                    samples_left =
                        GeneratePcm16(voice, scratch_.data(), new_samples);
                    break;
                default:
                    samples_left = 0;
                    std::fill_n(scratch_.begin(), new_samples, 0);
                    break;
            }

            if (samples_left != 0 || voice->ended)
            {
                voice->ended = false;

                // Ramp whatever is left of the block down to silence instead of
                // cutting hard, which is what the chip's output filter does.
                const std::uint32_t base = new_samples - samples_left;
                std::int32_t tail =
                    (base == 0) ? curr : scratch_[base - 1];
                for (std::uint32_t i = 0; i < samples_left; ++i)
                {
                    if (tail < 0)
                    {
                        tail = -((-tail * 15) >> 4);
                    }
                    else if (tail > 0)
                    {
                        tail = (tail * 15) >> 4;
                    }
                    scratch_[base + i] = static_cast<std::int16_t>(tail);
                }

                if (base != 0)
                {
                    // MAME defers this to a zero-delay timer so the IRQ lands on
                    // the next CPU slice. The PIU10 board leaves the IRQ line
                    // unwired, so only the status bit matters here.
                    voice->playing = false;
                    status_register_ |=
                        static_cast<std::uint8_t>(1U << index);
                }
            }

            // new_samples is sized so the resampler consumes exactly that many
            // slots, but clamping it at kMaxSampleChunk could in principle starve
            // the walk, so hold the last value rather than running off the buffer.
            const auto next_scratch = [&]() -> std::int16_t {
                if (scratch_index >= new_samples + kScratchSlack)
                {
                    return scratch_[new_samples + kScratchSlack - 1U];
                }
                return scratch_[scratch_index++];
            };

            prev = curr;
            curr = next_scratch();

            while (remaining > 0)
            {
                while (remaining > 0 && voice->output_pos < kFracOne)
                {
                    emit(prev, curr);
                }
                if (voice->output_pos >= kFracOne)
                {
                    voice->output_pos -= kFracOne;
                    prev = curr;
                    curr = next_scratch();
                }
            }

            voice->last_sample = prev;
            voice->curr_sample = curr;
        }
    }

    for (std::size_t i = 0; i < sample_count; ++i)
    {
        const std::int32_t scaled = accumulator_[i] / kMixNormalizeDivisor;
        const std::int32_t clamped = std::clamp(scaled, -32768, 32767);
        output[i] = static_cast<std::int16_t>(clamped);
        if (clamped != 0)
        {
            ++non_silent_sample_count_;
            peak_amplitude_ = std::max(peak_amplitude_, std::abs(clamped));
        }
    }
    generated_frame_count_ += frames;
}

}  // namespace repiu::sound
