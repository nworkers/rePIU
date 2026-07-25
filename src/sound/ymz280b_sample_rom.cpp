#include "repiu/sound/ymz280b_sample_rom.h"

#include "repiu/assets/rom_zip_archive.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace repiu::sound
{

Ymz280bSampleRom LoadPumpIt1SampleRom(const std::filesystem::path& rom_zip_path)
{
    Ymz280bSampleRom result;

    const assets::RomZipEntry entry =
        assets::ExtractRomZipEntry(rom_zip_path, kPumpIt1SampleRomEntry);
    if (!entry.valid)
    {
        result.message = entry.message;
        return result;
    }
    if (entry.data.size() > kYmz280bSampleRomBytes)
    {
        std::ostringstream stream;
        stream << "sample ROM '" << kPumpIt1SampleRomEntry << "' is "
               << entry.data.size() << " bytes, larger than the "
               << kYmz280bSampleRomBytes << " byte address space";
        result.message = stream.str();
        return result;
    }

    // Unpopulated space on the board floats high, and MAME models it with
    // ROMREGION_ERASEFF. Anything that walks past the end of the dumped half must
    // therefore read 0xFF rather than 0x00, which decodes very differently.
    result.data.assign(kYmz280bSampleRomBytes, 0xFFU);
    std::copy(entry.data.begin(), entry.data.end(), result.data.begin());
    result.crc32 = entry.crc32;
    result.crc32_matches_reference = entry.crc32 == kPumpIt1SampleRomCrc32;
    result.valid = true;

    std::ostringstream stream;
    stream << "loaded " << kPumpIt1SampleRomEntry << " (" << entry.data.size()
           << " bytes) into a " << kYmz280bSampleRomBytes
           << " byte space, crc32 0x" << std::hex << std::setw(8)
           << std::setfill('0') << entry.crc32 << std::dec
           << (result.crc32_matches_reference ? " (matches MAME pumpit1)"
                                              : " (DOES NOT match MAME pumpit1)");
    result.message = stream.str();
    return result;
}

}  // namespace repiu::sound
