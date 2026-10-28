#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>

namespace testing
{

// Each test process gets its own directory, even when several CI jobs share the same TEMP.
struct TempDirectory {
    std::filesystem::path path;

    explicit TempDirectory(std::string_view name)
    {
        static std::atomic<unsigned long long> next{0};
        for (;;) {
            const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
            path = std::filesystem::temp_directory_path() /
                (std::string(name) + '_' + std::to_string(stamp) + '_' + std::to_string(next++));
            std::error_code error;
            if (std::filesystem::create_directory(path, error)) {
                return;
            }
            if (error) {
                throw std::filesystem::filesystem_error("Cannot create test directory", path, error);
            }
        }
    }

    ~TempDirectory()
    {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }

    TempDirectory(const TempDirectory &) = delete;
    TempDirectory &operator=(const TempDirectory &) = delete;
};

} // namespace testing
