#pragma once

#include "repiu/platform/win32/ymz280b_audio_out.h"

#include <cstdint>

namespace repiu::platform::win32
{

// PIU10 ISA board sound window. MAME wires the YMZ280B with
//   map(0x00, 0x03).rw("ymz", read, write).umask16(0x00ff)
// on a 16-bit bus, so only the low byte of each 16-bit word is decoded:
// 0x02A0 is chip offset 0 (register select / readback latch) and 0x02A2 is chip
// offset 1 (register data / status). 0x02A1 and 0x02A3 are the undecoded high
// bytes.
inline constexpr std::uint16_t kPiu10SoundPortBase = 0x02A0U;
inline constexpr std::uint16_t kPiu10SoundPortEnd = 0x02A3U;

bool IsPiu10SoundPort(std::uint16_t port);

// Decomposes an access into ISA byte lanes and forwards the decoded ones. A
// 32-bit OUT to 0x02A0 therefore performs a register select followed by a data
// write, exactly as the board's two 16-bit bus cycles would.
void WritePiu10SoundPort(Ymz280bAudioOut* audio,
                         std::uint16_t port,
                         std::uint32_t width,
                         std::uint32_t value);

std::uint32_t ReadPiu10SoundPort(Ymz280bAudioOut* audio,
                                 std::uint16_t port,
                                 std::uint32_t width);

}  // namespace repiu::platform::win32
