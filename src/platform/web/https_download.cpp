#include "repiu/platform/https_download.h"

#if defined(__EMSCRIPTEN__)

namespace repiu::platform
{

// Issue #48. A page updates by being reloaded; the browser build has no
// install folder to replace files in, so the launcher's check never runs here.
HttpsDownloadResult HttpsDownloadToFile(const HttpsDownloadRequest&,
                                        const std::filesystem::path&,
                                        std::string* error)
{
    *error = "downloads are not available in the web build";
    return HttpsDownloadResult::kUnavailable;
}

}  // namespace repiu::platform

#endif  // __EMSCRIPTEN__
