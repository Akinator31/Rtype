#include "rtype/render/Paths.hpp"

#include <raylib.h>

#include <cstdlib>
#include <memory>
#include <string>

namespace rtype::render
{

std::filesystem::path applicationDirectory()
{
    // raylib returns UTF-8. Going through char8_t keeps non-ASCII paths intact on Windows, where a char
    // string would be read with the local code page instead.
    const char *directory = GetApplicationDirectory();
    const std::u8string utf8(reinterpret_cast<const char8_t *>(directory));
    return std::filesystem::path(utf8);
}

std::filesystem::path userDataDirectory()
{
#ifdef _WIN32
    // The wide variable keeps a non-ASCII user name intact
    wchar_t *appData = nullptr;
    std::size_t length = 0;
    if (_wdupenv_s(&appData, &length, L"APPDATA") == 0 && appData != nullptr) {
        const std::unique_ptr<wchar_t, decltype(&std::free)> owned(appData, &std::free);
        if (*appData != L'\0') {
            return std::filesystem::path(appData) / "R-Type";
        }
    }
#else
    if (const char *data = std::getenv("XDG_DATA_HOME"); data != nullptr && *data != '\0') {
        return std::filesystem::path(data) / "r-type";
    }
    if (const char *home = std::getenv("HOME"); home != nullptr && *home != '\0') {
        return std::filesystem::path(home) / ".local" / "share" / "r-type";
    }
#endif
    return applicationDirectory();
}

} // namespace rtype::render
