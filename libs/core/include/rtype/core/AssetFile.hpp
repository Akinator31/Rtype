#pragma once

#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace rtype::core
{

inline std::vector<unsigned char> readAssetFile(const std::filesystem::path &path)
{
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        throw std::runtime_error("Cannot read asset " + path.string());
    }
    const std::streamoff length = input.tellg();
    if (length < 0 || static_cast<unsigned long long>(length) >
                          static_cast<unsigned long long>(std::numeric_limits<int>::max())) {
        throw std::runtime_error("Invalid asset size " + path.string());
    }
    std::vector<unsigned char> bytes(static_cast<std::size_t>(length));
    input.seekg(0);
    if (!bytes.empty() && !input.read(reinterpret_cast<char *>(bytes.data()), length)) {
        throw std::runtime_error("Cannot read asset " + path.string());
    }
    return bytes;
}

}
