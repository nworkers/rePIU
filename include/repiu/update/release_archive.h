#ifndef REPIU_UPDATE_RELEASE_ARCHIVE_H_
#define REPIU_UPDATE_RELEASE_ARCHIVE_H_

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace repiu::update
{

// Issue #48. The two shapes a release archive comes in: the Win32 zip, with
// its files at the root, and the Linux tar.gz, with them under one
// `rePIU-v<ver>-linux-<arch>/` folder.
enum class ReleaseArchiveFormat
{
    kZip,
    kTarGz,
};

// From the asset name's extension; nothing for any other.
[[nodiscard]] std::optional<ReleaseArchiveFormat> ReleaseArchiveFormatForName(
    std::string_view name);

// Unpacks `archive` into `staging`, which is created and must not exist yet,
// and lists the files written as paths relative to it. When every entry sits
// under one top folder, that folder is stripped, so both formats stage the
// install folder's own layout. Absolute paths, `..`, links and device entries
// fail the whole archive -- a release has none of them -- and so does more
// than `kMaxReleaseArchiveBytes` of content. The executable bit comes from the
// tar mode; zip entries carry none.
inline constexpr std::uint64_t kMaxReleaseArchiveBytes = 512ULL << 20;

bool ExtractReleaseArchive(const std::vector<std::uint8_t>& archive,
                           ReleaseArchiveFormat format,
                           const std::filesystem::path& staging,
                           std::vector<std::filesystem::path>* files,
                           std::string* error);

}  // namespace repiu::update

#endif  // REPIU_UPDATE_RELEASE_ARCHIVE_H_
