#ifndef REPIU_SOUND_YMZ280B_DEVICE_H_
#define REPIU_SOUND_YMZ280B_DEVICE_H_

#include <cstddef>
#include <cstdint>
#include <vector>

namespace repiu::sound
{

// Yamaha YMZ280B, the 8-voice PCM/ADPCM decoder on the PIU10 ISA board.
//
// The register file, address decoding, ADPCM state machine, and mixing are ported
// from MAME src/devices/sound/ymz280b.cpp (BSD-3-Clause, Aaron Giles), which is
// license-compatible with this project. Behavioral differences from MAME are
// limited to two deliberate ones, both documented in
// docs/design/20260725-290-ymz280b-sound-emulation.md:
//   * no IRQ delivery, because the line is unwired on this board
//   * no 0.5 speaker route attenuation, since this is not mixed against other
//     MAME sound sources
//
// This class is not thread-safe. The platform backend owns the lock, because only
// it knows when generation and register writes can race.
class Ymz280bDevice
{
public:
    // XTAL feeding the chip on the xtom3d/PIU10 board.
    static constexpr std::uint32_t kPiu10MasterClockHz = 16934400U;

    static constexpr int kVoiceCount = 8;

    // Voice `mode` values as programmed in register 0x01 bits 5-6.
    static constexpr int kModeAdpcm = 1;
    static constexpr int kModePcm8 = 2;
    static constexpr int kModePcm16 = 3;

    // Takes ownership of the sample address space. `rom` is indexed by byte
    // address; anything beyond its end reads back as 0xFF like unpopulated board
    // space.
    void Initialize(std::vector<std::uint8_t> rom,
                    std::uint32_t master_clock_hz = kPiu10MasterClockHz);

    void Reset();

    // Rate at which Generate() produces frames. The datasheet derives it as
    // master_clock / 384 for ADPCM, doubled because PCM modes run at twice that.
    // For the 16.9344 MHz board XTAL this is 88200 Hz.
    std::uint32_t output_sample_rate() const { return output_sample_rate_; }

    bool initialized() const { return !rom_.empty(); }

    // Port-facing register interface. Offsets match the chip's two-register
    // interface: offset 0 selects a register, offset 1 reads status or writes the
    // selected register's data.
    void WriteRegisterSelect(std::uint8_t value);
    void WriteRegisterData(std::uint8_t value);
    std::uint8_t ReadExternalMemory();
    std::uint8_t ReadStatus();

    // Produces `frames` interleaved stereo s16 frames at output_sample_rate().
    // `output` must hold frames * 2 samples.
    void Generate(std::int16_t* output, std::uint32_t frames);

    // True while any voice is producing output. Used only for logging.
    bool AnyVoicePlaying() const;

    // Cumulative counters for verification, never for behavior.
    std::uint64_t register_write_count() const { return register_write_count_; }
    std::uint64_t key_on_count() const { return key_on_count_; }
    std::uint64_t generated_frame_count() const { return generated_frame_count_; }
    std::uint64_t non_silent_sample_count() const
    {
        return non_silent_sample_count_;
    }
    std::int32_t peak_amplitude() const { return peak_amplitude_; }

    // Reported once per key-on so callers can log what the guest asked for
    // without reaching into voice state.
    struct KeyOnEvent
    {
        int voice = 0;
        int mode = 0;
        std::uint32_t start_byte_address = 0;
        std::uint32_t stop_byte_address = 0;
        std::uint32_t sample_rate_hz = 0;
        int level = 0;
        int pan = 0;
        bool looping = false;
    };

    // Moves the pending key-on events out of the device. Returns them in the order
    // they occurred; the internal queue is bounded and drops the oldest on
    // overflow so a runaway guest cannot grow it without bound.
    std::vector<KeyOnEvent> TakeKeyOnEvents();

private:
    struct Voice
    {
        bool playing = false;
        bool ended = false;
        bool keyon = false;
        bool looping = false;
        int mode = 0;

        int fnum = 0;
        int level = 0;
        int pan = 0;

        // Addresses are kept in the chip's native nibble units: the register
        // bytes compose as (high << 17) | (mid << 9) | (low << 1), so a byte
        // address is always `value / 2`.
        std::uint32_t start = 0;
        std::uint32_t stop = 0;
        std::uint32_t loop_start = 0;
        std::uint32_t loop_end = 0;
        std::uint32_t position = 0;

        int signal = 0;
        int step = 0;
        int loop_signal = 0;
        int loop_step = 0;
        int loop_count = 0;

        int output_left = 0;
        int output_right = 0;
        std::int32_t output_pos = 0;
        std::int16_t last_sample = 0;
        std::int16_t curr_sample = 0;
        std::uint32_t output_step = 0;
    };

    std::uint8_t ReadByte(std::uint32_t byte_address) const;
    void WriteToRegister(std::uint8_t data);
    void UpdateStep(Voice* voice);
    void UpdateVolumes(Voice* voice);
    void RecordKeyOn(int index, const Voice& voice);

    std::uint32_t GenerateAdpcm(Voice* voice, std::int16_t* buffer,
                                std::uint32_t samples);
    std::uint32_t GeneratePcm8(Voice* voice, std::int16_t* buffer,
                               std::uint32_t samples);
    std::uint32_t GeneratePcm16(Voice* voice, std::int16_t* buffer,
                                std::uint32_t samples);

    std::vector<std::uint8_t> rom_;
    std::uint32_t master_clock_hz_ = kPiu10MasterClockHz;
    std::uint32_t output_sample_rate_ = 0;

    Voice voices_[kVoiceCount];

    std::uint8_t current_register_ = 0;
    std::uint8_t status_register_ = 0;
    std::uint8_t irq_mask_ = 0;
    bool irq_enable_ = false;
    bool keyon_enable_ = false;
    bool external_memory_enable_ = false;
    std::uint32_t external_memory_address_ = 0;
    std::uint32_t external_memory_address_high_ = 0;
    std::uint32_t external_memory_address_mid_ = 0;
    std::uint8_t external_read_latch_ = 0;

    std::vector<std::int16_t> scratch_;
    std::vector<std::int32_t> accumulator_;
    std::vector<KeyOnEvent> key_on_events_;

    std::uint64_t register_write_count_ = 0;
    std::uint64_t key_on_count_ = 0;
    std::uint64_t generated_frame_count_ = 0;
    std::uint64_t non_silent_sample_count_ = 0;
    std::int32_t peak_amplitude_ = 0;
};

}  // namespace repiu::sound

#endif  // REPIU_SOUND_YMZ280B_DEVICE_H_
