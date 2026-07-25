#ifndef REPIU_SOUND_YMZ280B_SAMPLE_ROM_H_
#define REPIU_SOUND_YMZ280B_SAMPLE_ROM_H_

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace repiu::sound
{

// Address space the YMZ280B sees on the PIU10 board. MAME declares the region as
// ROM_REGION(0x400000, "isa1:pumpitup_io_sound:ymz", ROMREGION_ERASEFF) and loads
// only the 2 MiB `piu10.u9` at offset 0, so the upper half reads back as 0xFF.
inline constexpr std::size_t kYmz280bSampleRomBytes = 0x400000U;
inline constexpr const char* kPumpIt1SampleRomEntry = "piu10.u9";

// Expected CRC32 of `piu10.u9` from the MAME `pumpit1` ROM definition. Used only
// to report whether the loaded set matches the driver, never to reject a load.
inline constexpr std::uint32_t kPumpIt1SampleRomCrc32 = 0x9C436CFAU;

struct Ymz280bSampleRom
{
    bool valid = false;
    std::vector<std::uint8_t> data;
    std::uint32_t crc32 = 0;
    bool crc32_matches_reference = false;
    std::string message;
};

// Builds the 4 MiB sample address space from `roms/pumpit1.zip` by filling it with
// 0xFF and copying `piu10.u9` to offset 0.
Ymz280bSampleRom LoadPumpIt1SampleRom(const std::filesystem::path& rom_zip_path);

}  // namespace repiu::sound

#endif  // REPIU_SOUND_YMZ280B_SAMPLE_ROM_H_
