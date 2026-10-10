#include "launcher_update_probe.h"

#include "repiu/update/launcher_updater.h"
#include "repiu/update/release_archive.h"
#include "repiu/update/release_info.h"
#include "repiu/update/semantic_version.h"
#include "repiu/update/sha256.h"
#include "repiu/update/update_install.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace repiu::tools
{
namespace
{

namespace fs = std::filesystem;
using Bytes = std::vector<std::uint8_t>;

const char* Flag(bool value)
{
    return value ? "true" : "false";
}

fs::path Scratch(const std::string& name)
{
    std::error_code error;
    const fs::path root = fs::temp_directory_path(error) /
        ("repiu-update-probe-" + name);
    fs::remove_all(root, error);
    fs::create_directories(root, error);
    return root;
}

void WriteText(const fs::path& path, const std::string& text)
{
    std::error_code error;
    fs::create_directories(path.parent_path(), error);
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
}

std::string ReadText(const fs::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    std::ostringstream text;
    text << stream.rdbuf();
    return text.str();
}

void PutLe(Bytes* out, std::uint32_t value, int bytes)
{
    for (int index = 0; index < bytes; ++index)
    {
        out->push_back(static_cast<std::uint8_t>(value >> (8 * index)));
    }
}

// CRC-32 (ISO 3309, reflected 0xEDB88320), which gzip and zip both carry.
std::uint32_t Crc32(const Bytes& data)
{
    std::uint32_t crc = 0xFFFFFFFFU;
    for (std::uint8_t byte : data)
    {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit)
        {
            crc = (crc >> 1) ^ (0xEDB88320U & (0U - (crc & 1U)));
        }
    }
    return ~crc;
}

struct TarFile
{
    std::string name;
    std::string data;
    std::uint32_t mode = 0644;
    char type = '0';
};

// A tar numeric field: zero-padded octal filling all but the last byte.
void Octal(char* field, std::size_t size, std::uint64_t value)
{
    for (std::size_t index = size - 1U; index > 0U; --index)
    {
        field[index - 1U] = static_cast<char>('0' + (value & 7U));
        value >>= 3;
    }
    field[size - 1U] = '\0';
}

Bytes Tar(const std::vector<TarFile>& files)
{
    Bytes tar;
    for (const TarFile& file : files)
    {
        char header[512] = {};
        std::strncpy(header, file.name.c_str(), 99);
        Octal(header + 100, 8, file.mode);
        Octal(header + 108, 8, 0);
        Octal(header + 116, 8, 0);
        Octal(header + 124, 12, file.data.size());
        Octal(header + 136, 12, 0);
        header[156] = file.type;
        std::memcpy(header + 257, "ustar", 6);
        std::memcpy(header + 263, "00", 2);
        std::memset(header + 148, ' ', 8);
        unsigned checksum = 0;
        for (unsigned char byte : header)
        {
            checksum += byte;
        }
        std::snprintf(header + 148, 8, "%06o", checksum);
        tar.insert(tar.end(), header, header + 512);
        tar.insert(tar.end(), file.data.begin(), file.data.end());
        tar.resize((tar.size() + 511U) / 512U * 512U, 0);
    }
    tar.resize(tar.size() + 1024U, 0);
    return tar;
}

// A gzip member whose deflate stream is stored blocks: no compressor needed.
Bytes Gzip(const Bytes& data)
{
    Bytes out = {0x1F, 0x8B, 8, 0, 0, 0, 0, 0, 0, 3};
    std::size_t position = 0;
    do
    {
        const std::size_t length = std::min<std::size_t>(data.size() - position,
                                                         65535U);
        const bool final_block = position + length == data.size();
        out.push_back(final_block ? 1 : 0);
        PutLe(&out, static_cast<std::uint32_t>(length), 2);
        PutLe(&out, static_cast<std::uint32_t>(~length & 0xFFFFU), 2);
        out.insert(out.end(), data.begin() + static_cast<std::ptrdiff_t>(position),
                   data.begin() + static_cast<std::ptrdiff_t>(position + length));
        position += length;
    } while (position < data.size());
    PutLe(&out, Crc32(data), 4);
    PutLe(&out, static_cast<std::uint32_t>(data.size()), 4);
    return out;
}

// A zip with stored entries, written the way the format describes.
Bytes Zip(const std::vector<std::pair<std::string, std::string>>& files)
{
    Bytes out;
    Bytes central;
    for (const auto& [name, text] : files)
    {
        const Bytes data(text.begin(), text.end());
        const std::uint32_t offset = static_cast<std::uint32_t>(out.size());
        const std::uint32_t crc = Crc32(data);
        PutLe(&out, 0x04034B50U, 4);
        PutLe(&out, 20, 2);
        PutLe(&out, 0, 2);
        PutLe(&out, 0, 2);  // stored
        PutLe(&out, 0, 4);  // time, date
        PutLe(&out, crc, 4);
        PutLe(&out, static_cast<std::uint32_t>(data.size()), 4);
        PutLe(&out, static_cast<std::uint32_t>(data.size()), 4);
        PutLe(&out, static_cast<std::uint32_t>(name.size()), 2);
        PutLe(&out, 0, 2);
        out.insert(out.end(), name.begin(), name.end());
        out.insert(out.end(), data.begin(), data.end());

        PutLe(&central, 0x02014B50U, 4);
        PutLe(&central, 20, 2);
        PutLe(&central, 20, 2);
        PutLe(&central, 0, 2);
        PutLe(&central, 0, 2);
        PutLe(&central, 0, 4);
        PutLe(&central, crc, 4);
        PutLe(&central, static_cast<std::uint32_t>(data.size()), 4);
        PutLe(&central, static_cast<std::uint32_t>(data.size()), 4);
        PutLe(&central, static_cast<std::uint32_t>(name.size()), 2);
        PutLe(&central, 0, 2);
        PutLe(&central, 0, 2);
        PutLe(&central, 0, 2);
        PutLe(&central, 0, 2);
        PutLe(&central, 0, 4);
        PutLe(&central, offset, 4);
        central.insert(central.end(), name.begin(), name.end());
    }
    const std::uint32_t central_offset = static_cast<std::uint32_t>(out.size());
    out.insert(out.end(), central.begin(), central.end());
    PutLe(&out, 0x06054B50U, 4);
    PutLe(&out, 0, 2);
    PutLe(&out, 0, 2);
    PutLe(&out, static_cast<std::uint32_t>(files.size()), 2);
    PutLe(&out, static_cast<std::uint32_t>(files.size()), 2);
    PutLe(&out, static_cast<std::uint32_t>(central.size()), 4);
    PutLe(&out, central_offset, 4);
    PutLe(&out, 0, 2);
    return out;
}

std::string HashOf(const std::string& text)
{
    update::Sha256 hash;
    hash.Update(text.data(), text.size());
    return update::FormatSha256(hash.Finish());
}

Bytes ReleaseTarGz()
{
    return Gzip(Tar({
        {"rePIU-v0.0.9-linux-x64/", "", 0755, '5'},
        {"rePIU-v0.0.9-linux-x64/repiu", "new-binary", 0755, '0'},
        {"rePIU-v0.0.9-linux-x64/VERSION", "0.0.9\n", 0644, '0'},
    }));
}

std::string ReleaseJson(const std::string& digest)
{
    return std::string(R"({
  "url": "https://api.github.com/repos/reexec/rePIU/releases/1",
  "html_url": "https://github.com/reexec/rePIU/releases/tag/v0.0.9",
  "tag_name": "v0.0.9",
  "name": "rePIU v0.0.9 — test",
  "draft": false,
  "author": {"login": "someone", "id": 1, "site_admin": false},
  "assets": [
    {"name": "rePIU-v0.0.9-win32.zip", "size": 100,
     "digest": "sha256:)") + std::string(64, 'a') + R"(",
     "browser_download_url": "https://example.invalid/win32.zip",
     "uploader": {"login": "github-actions[bot]"}},
    {"name": "rePIU-v0.0.9-linux-x64.tar.gz", "size": )" +
        std::to_string(ReleaseTarGz().size()) + R"(,
     "digest": "sha256:)" + digest + R"(",
     "browser_download_url": "https://example.invalid/linux-x64.tar.gz"},
    {"name": "rePIU-v0.0.9-linux-i386.tar.gz", "size": 300,
     "browser_download_url": "https://example.invalid/linux-i386.tar.gz"}
  ]
})";
}

bool WaitFor(update::LauncherUpdater* updater, update::UpdateStage busy)
{
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (updater->Snapshot().stage == busy)
    {
        if (std::chrono::steady_clock::now() > deadline)
        {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return true;
}

}  // namespace

bool RunLauncherUpdateProbe()
{
    using update::ParseSemanticVersion;
    using update::SemanticVersion;

    // Versions: the tag and VERSION forms, numeric order, and refusals.
    const auto v214 = ParseSemanticVersion("0.0.214");
    const auto tagged = ParseSemanticVersion("v0.0.214");
    const auto file = ParseSemanticVersion(" 0.0.9\n");
    const bool versions = v214.has_value() && tagged == v214 &&
        file.has_value() &&
        *ParseSemanticVersion("0.0.10") > *ParseSemanticVersion("0.0.9") &&
        *ParseSemanticVersion("1.0.0") > *ParseSemanticVersion("0.99.99") &&
        !ParseSemanticVersion("1.2").has_value() &&
        !ParseSemanticVersion("1.2.3-rc1").has_value() &&
        !ParseSemanticVersion("a.b.c").has_value() &&
        !ParseSemanticVersion("").has_value() &&
        update::FormatSemanticVersion(*v214) == "0.0.214";

    // SHA-256: FIPS 180-4 test vectors, the last crossing many blocks.
    update::Sha256 million;
    const std::string chunk(1000, 'a');
    for (int index = 0; index < 1000; ++index)
    {
        million.Update(chunk.data(), chunk.size());
    }
    const bool sha256 =
        HashOf("") ==
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855" &&
        HashOf("abc") ==
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" &&
        update::FormatSha256(million.Finish()) ==
            "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0";

    // The release response, nested objects and an escape included.
    const Bytes tar_gz = ReleaseTarGz();
    update::Sha256 archive_hash;
    archive_hash.Update(tar_gz.data(), tar_gz.size());
    const std::string tar_gz_digest = update::FormatSha256(archive_hash.Finish());
    update::ReleaseInfo release;
    std::string error;
    const bool parsed =
        update::ParseLatestRelease(ReleaseJson(tar_gz_digest), &release, &error);
    const auto win_name =
        update::ReleaseAssetName(release.version, "Win", "x86");
    const auto x64_name =
        update::ReleaseAssetName(release.version, "Linux", "x64");
    const auto i386_name =
        update::ReleaseAssetName(release.version, "Linux", "x86");
    const update::ReleaseAsset* x64 =
        x64_name.has_value() ? update::FindReleaseAsset(release, *x64_name)
                             : nullptr;
    const update::ReleaseAsset* i386 =
        i386_name.has_value() ? update::FindReleaseAsset(release, *i386_name)
                              : nullptr;
    update::ReleaseInfo not_found;
    std::string not_found_error;
    update::ReleaseInfo bad_tag;
    std::string bad_tag_error;
    const bool release_info = parsed && release.tag == "v0.0.9" &&
        release.version == SemanticVersion{0, 0, 9} &&
        release.assets.size() == 3U &&
        release.page_url ==
            "https://github.com/reexec/rePIU/releases/tag/v0.0.9" &&
        win_name == std::optional<std::string>("rePIU-v0.0.9-win32.zip") &&
        x64 != nullptr && x64->sha256 == tar_gz_digest &&
        x64->size == tar_gz.size() && i386 != nullptr && i386->sha256.empty() &&
        !update::ReleaseAssetName(release.version, "Linux", "arm64")
             .has_value() &&
        !update::ParseLatestRelease(R"({"message": "Not Found"})", &not_found,
                                    &not_found_error) &&
        not_found_error == "GitHub: Not Found" &&
        !update::ParseLatestRelease(R"({"tag_name": "nightly"})", &bad_tag,
                                    &bad_tag_error) &&
        !update::ParseLatestRelease("{\"tag_name\": ", &bad_tag,
                                    &bad_tag_error);

    // tar.gz: the top folder stripped, the executable bit kept.
    const fs::path tar_root = Scratch("tar");
    std::vector<fs::path> tar_files;
    const bool tar_extracted = update::ExtractReleaseArchive(
        tar_gz, update::ReleaseArchiveFormat::kTarGz, tar_root / "staged",
        &tar_files, &error);
    bool executable = true;
#if !defined(_WIN32)
    std::error_code perm_error;
    executable = (fs::status(tar_root / "staged" / "repiu", perm_error)
                      .permissions() &
                  fs::perms::owner_exec) != fs::perms::none &&
        (fs::status(tar_root / "staged" / "VERSION", perm_error)
             .permissions() &
         fs::perms::owner_exec) == fs::perms::none;
#endif
    const bool tar_ok = tar_extracted && tar_files.size() == 2U &&
        ReadText(tar_root / "staged" / "repiu") == "new-binary" &&
        ReadText(tar_root / "staged" / "VERSION") == "0.0.9\n" && executable;

    // zip: files at the root stay at the root.
    const fs::path zip_root = Scratch("zip");
    std::vector<fs::path> zip_files;
    const bool zip_ok =
        update::ExtractReleaseArchive(
            Zip({{"repiu.exe", "win-binary"}, {"VERSION", "0.0.9\r\n"}}),
            update::ReleaseArchiveFormat::kZip, zip_root / "staged", &zip_files,
            &error) &&
        zip_files.size() == 2U &&
        ReadText(zip_root / "staged" / "repiu.exe") == "win-binary";

    // Anything that could leave the staging folder fails before writing.
    const fs::path unsafe_root = Scratch("unsafe");
    std::vector<fs::path> unsafe_files;
    const bool unsafe =
        !update::ExtractReleaseArchive(
            Gzip(Tar({{"top/ok", "x"}, {"top/../../evil", "x"}})),
            update::ReleaseArchiveFormat::kTarGz, unsafe_root / "a",
            &unsafe_files, &error) &&
        !fs::exists(unsafe_root / "a") &&
        !update::ExtractReleaseArchive(
            Gzip(Tar({{"top/link", "", 0777, '2'}})),
            update::ReleaseArchiveFormat::kTarGz, unsafe_root / "b",
            &unsafe_files, &error) &&
        !update::ExtractReleaseArchive(
            Zip({{"..\\evil.exe", "x"}}), update::ReleaseArchiveFormat::kZip,
            unsafe_root / "c", &unsafe_files, &error) &&
        !update::ExtractReleaseArchive(
            Zip({{"/abs", "x"}}), update::ReleaseArchiveFormat::kZip,
            unsafe_root / "d", &unsafe_files, &error) &&
        update::ReleaseArchiveFormatForName("x.tar.gz") ==
            update::ReleaseArchiveFormat::kTarGz &&
        !update::ReleaseArchiveFormatForName("x.7z").has_value();

    // Install: VERSION decides, user data stays, the old files go aside and
    // are removed on the next start.
    const fs::path install = Scratch("install");
    WriteText(install / "repiu", "old-binary");
    WriteText(install / "VERSION", "0.0.8\n");
    WriteText(install / "roms" / "keep.zip", "rom");
    WriteText(install / "cfg" / "repiu.ini", "[Video]\n");
    std::string reason;
    const fs::path bare = Scratch("bare");
    const bool condition =
        update::CanInstallInto(install, SemanticVersion{0, 0, 8}, &reason) &&
        !update::CanInstallInto(install, SemanticVersion{0, 0, 7}, &reason) &&
        !update::CanInstallInto(bare, SemanticVersion{0, 0, 8}, &reason);
    const bool installed = update::InstallStagedFiles(
        tar_root / "staged", tar_files, install, &error);
    const bool replaced = installed &&
        ReadText(install / "repiu") == "new-binary" &&
        ReadText(install / "VERSION") == "0.0.9\n" &&
        ReadText(install / "repiu.repiu-old") == "old-binary" &&
        ReadText(install / "roms" / "keep.zip") == "rom" &&
        ReadText(install / "cfg" / "repiu.ini") == "[Video]\n" &&
        !fs::exists(tar_root / "staged");
    const std::size_t removed = update::RemovePreviousInstallLeftovers(install);
    const bool cleaned = removed == 2U && !fs::exists(install / "repiu.repiu-old") &&
        !fs::exists(install / "VERSION.repiu-old") &&
        !fs::exists(install / ".repiu-old-files");

    // A failure part way through puts every file back.
    const fs::path rollback = Scratch("rollback");
    WriteText(rollback / "install" / "a", "old-a");
    WriteText(rollback / "install" / "b", "old-b");
    // A directory where b's aside name must go makes the second step fail.
    WriteText(rollback / "install" / "b.repiu-old" / "blocker", "x");
    WriteText(rollback / "staged" / "a", "new-a");
    WriteText(rollback / "staged" / "b", "new-b");
    std::string rollback_error;
    const bool rolled_back =
        !update::InstallStagedFiles(rollback / "staged", {"a", "b"},
                                    rollback / "install", &rollback_error) &&
        ReadText(rollback / "install" / "a") == "old-a" &&
        ReadText(rollback / "install" / "b") == "old-b" &&
        ReadText(rollback / "staged" / "a") == "new-a" &&
        !fs::exists(rollback / "install" / "a.repiu-old");

    // The updater's states through a fake fetch: available, staged, installed;
    // up to date; and a download that does not match its digest.
    const auto fake_fetch = [&](const std::string& json, const Bytes& archive) {
        return [json, archive](const platform::HttpsDownloadRequest& request,
                               const fs::path& destination, std::string*) {
            std::ofstream stream(destination, std::ios::binary | std::ios::trunc);
            const std::string url = request.url;
            if (url.find("api.github.com") != std::string::npos)
            {
                stream << json;
            }
            else
            {
                stream.write(reinterpret_cast<const char*>(archive.data()),
                             static_cast<std::streamsize>(archive.size()));
            }
            return platform::HttpsDownloadResult::kOk;
        };
    };
    const fs::path flow = Scratch("flow");
    WriteText(flow / "repiu", "old-binary");
    WriteText(flow / "VERSION", "0.0.8\n");
    update::LauncherUpdaterConfig config;
    config.build_version = SemanticVersion{0, 0, 8};
    config.install_folder = flow;
    config.platform = "Linux";
    config.architecture = "x64";
    config.fetch = fake_fetch(ReleaseJson(tar_gz_digest), tar_gz);
    bool flow_ok = false;
    {
        update::LauncherUpdater updater(config);
        updater.StartCheck();
        const bool checked = WaitFor(&updater, update::UpdateStage::kChecking);
        const update::UpdateSnapshot available = updater.Snapshot();
        const bool started = updater.StartDownload();
        WaitFor(&updater, update::UpdateStage::kDownloading);
        WaitFor(&updater, update::UpdateStage::kVerifying);
        const bool staged =
            updater.Snapshot().stage == update::UpdateStage::kStaged;
        std::string install_error;
        flow_ok = checked && available.stage == update::UpdateStage::kAvailable &&
            available.installable && available.latest_version == "0.0.9" &&
            available.bytes_expected == tar_gz.size() && started && staged &&
            updater.Install(&install_error) &&
            ReadText(flow / "repiu") == "new-binary" &&
            !fs::exists(updater.WorkFolder());
    }
    config.build_version = SemanticVersion{0, 0, 9};
    bool up_to_date = false;
    {
        update::LauncherUpdater updater(config);
        updater.StartCheck();
        up_to_date = WaitFor(&updater, update::UpdateStage::kChecking) &&
            updater.Snapshot().stage == update::UpdateStage::kUpToDate &&
            !updater.StartDownload();
    }
    const fs::path mismatch = Scratch("mismatch");
    WriteText(mismatch / "VERSION", "0.0.8\n");
    config.build_version = SemanticVersion{0, 0, 8};
    config.install_folder = mismatch;
    config.fetch = fake_fetch(ReleaseJson(std::string(64, '0')), tar_gz);
    bool digest_refused = false;
    {
        update::LauncherUpdater updater(config);
        updater.StartCheck();
        WaitFor(&updater, update::UpdateStage::kChecking);
        updater.StartDownload();
        WaitFor(&updater, update::UpdateStage::kDownloading);
        WaitFor(&updater, update::UpdateStage::kVerifying);
        const update::UpdateSnapshot failed = updater.Snapshot();
        digest_refused = failed.stage == update::UpdateStage::kFailed &&
            failed.message.find("SHA-256") != std::string::npos &&
            ReadText(mismatch / "VERSION") == "0.0.8\n";
    }

    std::error_code cleanup_error;
    for (const char* name : {"tar", "zip", "unsafe", "install", "bare",
                             "rollback", "flow", "mismatch"})
    {
        fs::remove_all(fs::temp_directory_path(cleanup_error) /
                           (std::string("repiu-update-probe-") + name),
                       cleanup_error);
    }

    const bool all = versions && sha256 && release_info && tar_ok && zip_ok &&
        unsafe && condition && replaced && cleaned && rolled_back && flow_ok &&
        up_to_date && digest_refused;
    std::cout << "launcher_update_versions=" << Flag(versions)
              << "\nlauncher_update_sha256=" << Flag(sha256)
              << "\nlauncher_update_release_info=" << Flag(release_info)
              << "\nlauncher_update_tar_gz=" << Flag(tar_ok)
              << "\nlauncher_update_zip=" << Flag(zip_ok)
              << "\nlauncher_update_unsafe_paths=" << Flag(unsafe)
              << "\nlauncher_update_install_condition=" << Flag(condition)
              << "\nlauncher_update_install=" << Flag(replaced)
              << "\nlauncher_update_leftovers=" << Flag(cleaned)
              << "\nlauncher_update_rollback=" << Flag(rolled_back)
              << "\nlauncher_update_flow=" << Flag(flow_ok)
              << "\nlauncher_update_up_to_date=" << Flag(up_to_date)
              << "\nlauncher_update_digest_refused=" << Flag(digest_refused)
              << "\nlauncher_update_all=" << Flag(all) << std::endl;
    return all;
}

}  // namespace repiu::tools
