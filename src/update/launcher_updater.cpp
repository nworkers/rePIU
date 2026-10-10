#include "repiu/update/launcher_updater.h"

#include "repiu/update/release_archive.h"
#include "repiu/update/sha256.h"
#include "repiu/update/update_install.h"

#include <chrono>
#include <fstream>
#include <iterator>
#include <sstream>
#include <system_error>
#include <utility>

namespace repiu::update
{
namespace
{

constexpr const char* kGitHubJson = "application/vnd.github+json";

bool ReadWholeFile(const std::filesystem::path& path,
                   std::vector<std::uint8_t>* bytes)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
    {
        return false;
    }
    bytes->assign(std::istreambuf_iterator<char>(stream),
                  std::istreambuf_iterator<char>());
    return !stream.bad();
}

}  // namespace

LauncherUpdater::LauncherUpdater(LauncherUpdaterConfig config)
    : config_(std::move(config))
{
    if (!config_.fetch)
    {
        config_.fetch = &platform::HttpsDownloadToFile;
    }
}

LauncherUpdater::~LauncherUpdater()
{
    cancel_.store(true, std::memory_order_relaxed);
    JoinWorker();
}

void LauncherUpdater::JoinWorker()
{
    if (worker_.joinable())
    {
        worker_.join();
    }
}

std::filesystem::path LauncherUpdater::WorkFolder() const
{
    return config_.install_folder / ".repiu-update";
}

void LauncherUpdater::Log(const std::string& line) const
{
    if (config_.log)
    {
        config_.log(line);
    }
}

void LauncherUpdater::Fail(const std::string& message)
{
    Log("update: " + message);
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_.stage = UpdateStage::kFailed;
    snapshot_.message = message;
}

void LauncherUpdater::StartCheck()
{
    JoinWorker();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot_ = UpdateSnapshot{};
        snapshot_.stage = UpdateStage::kChecking;
    }
    worker_ = std::thread([this]() { RunCheck(); });
}

void LauncherUpdater::RunCheck()
{
    const std::string url = "https://api.github.com/repos/" +
        config_.repository + "/releases/latest";
    std::error_code fs_error;
    const std::filesystem::path response =
        std::filesystem::temp_directory_path(fs_error) /
        ("repiu-latest-release-" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch()
                            .count()) +
         ".json");
    platform::HttpsDownloadRequest request;
    request.url = url.c_str();
    request.user_agent = config_.user_agent.c_str();
    request.accept = kGitHubJson;
    request.timeout_seconds = 20;
    request.cancel = &cancel_;
    std::string error;
    const auto fetched = config_.fetch(request, response, &error);
    if (fetched != platform::HttpsDownloadResult::kOk)
    {
        Fail("check failed: " + error);
        return;
    }
    std::vector<std::uint8_t> body;
    const bool read = ReadWholeFile(response, &body);
    std::filesystem::remove(response, fs_error);
    ReleaseInfo release;
    if (!read ||
        !ParseLatestRelease(std::string_view(
                                reinterpret_cast<const char*>(body.data()),
                                body.size()),
                            &release, &error))
    {
        Fail("check failed: " + (read ? error : std::string("no response")));
        return;
    }
    if (release.version <= config_.build_version)
    {
        Log("update: up to date (latest " + release.tag + ", this " +
            FormatSemanticVersion(config_.build_version) + ")");
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot_.stage = UpdateStage::kUpToDate;
        return;
    }

    UpdateSnapshot available;
    available.stage = UpdateStage::kAvailable;
    available.latest_version = FormatSemanticVersion(release.version);
    available.page_url = release.page_url;
    const auto name = ReleaseAssetName(release.version, config_.platform,
                                       config_.architecture);
    const ReleaseAsset* asset =
        name.has_value() ? FindReleaseAsset(release, *name) : nullptr;
    if (asset == nullptr || asset->sha256.empty() ||
        !ReleaseArchiveFormatForName(asset->name).has_value())
    {
        available.not_installable_reason =
            "the release has no verified archive for this build";
    }
    else
    {
        available.installable =
            CanInstallInto(config_.install_folder, config_.build_version,
                           &available.not_installable_reason);
        available.bytes_expected = asset->size;
        asset_ = *asset;
    }
    Log("update: " + release.tag + " available" +
        (available.installable ? std::string(" (installable)")
                               : " (" + available.not_installable_reason + ")"));
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_ = available;
}

bool LauncherUpdater::StartDownload()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const bool from_available = snapshot_.stage == UpdateStage::kAvailable;
        const bool retry = snapshot_.stage == UpdateStage::kFailed &&
            !snapshot_.latest_version.empty();
        if (!(from_available || retry) || !snapshot_.installable)
        {
            return false;
        }
        snapshot_.stage = UpdateStage::kDownloading;
        snapshot_.message.clear();
        snapshot_.bytes_received = 0;
    }
    JoinWorker();
    worker_ = std::thread([this]() { RunDownload(); });
    return true;
}

void LauncherUpdater::RunDownload()
{
    std::error_code fs_error;
    const std::filesystem::path work = WorkFolder();
    std::filesystem::remove_all(work, fs_error);
    std::filesystem::create_directories(work, fs_error);
    if (fs_error)
    {
        Fail("cannot create " + work.string() + ": " + fs_error.message());
        return;
    }
    const std::filesystem::path download = work / asset_.name;
    const std::filesystem::path staging = work / "staged";
    {
        std::lock_guard<std::mutex> lock(mutex_);
        download_path_ = download;
    }
    Log("update: downloading " + asset_.download_url);
    platform::HttpsDownloadRequest request;
    request.url = asset_.download_url.c_str();
    request.user_agent = config_.user_agent.c_str();
    request.timeout_seconds = 900;
    request.cancel = &cancel_;
    std::string error;
    const auto fetched = config_.fetch(request, download, &error);
    if (fetched != platform::HttpsDownloadResult::kOk)
    {
        Fail("download failed: " + error);
        return;
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot_.stage = UpdateStage::kVerifying;
    }
    const std::uint64_t size = std::filesystem::file_size(download, fs_error);
    const auto digest = Sha256OfFile(download);
    if (fs_error || size != asset_.size || !digest.has_value() ||
        *digest != asset_.sha256)
    {
        Fail("the download does not match the release's SHA-256");
        return;
    }
    std::vector<std::uint8_t> archive;
    std::vector<std::filesystem::path> files;
    if (!ReadWholeFile(download, &archive) ||
        !ExtractReleaseArchive(archive, *ReleaseArchiveFormatForName(asset_.name),
                               staging, &files, &error))
    {
        Fail("cannot unpack the update: " + error);
        return;
    }
    Log("update: staged " + std::to_string(files.size()) + " files from " +
        asset_.name);
    std::lock_guard<std::mutex> lock(mutex_);
    staging_path_ = staging;
    staged_files_ = std::move(files);
    snapshot_.stage = UpdateStage::kStaged;
}

UpdateSnapshot LauncherUpdater::Snapshot() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    UpdateSnapshot snapshot = snapshot_;
    if (snapshot.stage == UpdateStage::kDownloading && !download_path_.empty())
    {
        std::error_code error;
        const std::uintmax_t size =
            std::filesystem::file_size(download_path_, error);
        if (!error)
        {
            snapshot.bytes_received = size;
        }
    }
    return snapshot;
}

bool LauncherUpdater::Install(std::string* error)
{
    JoinWorker();
    std::filesystem::path staging;
    std::vector<std::filesystem::path> files;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (snapshot_.stage != UpdateStage::kStaged)
        {
            *error = "no update is staged";
            return false;
        }
        staging = staging_path_;
        files = staged_files_;
    }
    if (!InstallStagedFiles(staging, files, config_.install_folder, error))
    {
        Fail("install failed: " + *error);
        return false;
    }
    std::error_code fs_error;
    std::filesystem::remove_all(WorkFolder(), fs_error);
    Log("update: installed " + std::to_string(files.size()) + " files");
    return true;
}

}  // namespace repiu::update
