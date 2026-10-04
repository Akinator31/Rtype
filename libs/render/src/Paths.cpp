#include "rtype/render/Paths.hpp"

#include <raylib.h>

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

} // namespace rtype::render
