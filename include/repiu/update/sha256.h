#ifndef REPIU_UPDATE_SHA256_H_
#define REPIU_UPDATE_SHA256_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace repiu::update
{

// Issue #48. SHA-256 (FIPS 180-4), for checking a downloaded release archive
// against the `digest` GitHub reports for it. Incremental, so a file is hashed
// in blocks rather than read whole.
class Sha256
{
public:
    Sha256();

    void Update(const void* data, std::size_t size);
    // The digest of everything passed to Update. The object is spent after.
    [[nodiscard]] std::array<std::uint8_t, 32> Finish();

private:
    void Compress(const std::uint8_t* block);

    std::array<std::uint32_t, 8> state_;
    std::array<std::uint8_t, 64> buffer_{};
    std::size_t buffered_ = 0;
    std::uint64_t total_bytes_ = 0;
};

// Lower-case hexadecimal, as GitHub writes it after `sha256:`.
[[nodiscard]] std::string FormatSha256(
    const std::array<std::uint8_t, 32>& digest);

// The hex digest of a whole file, or nothing when it cannot be read.
[[nodiscard]] std::optional<std::string> Sha256OfFile(
    const std::filesystem::path& path);

}  // namespace repiu::update

#endif  // REPIU_UPDATE_SHA256_H_
