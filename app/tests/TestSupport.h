// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "ThirdParty/doctest.h"

#include <unistd.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace Dup::Tests
{
/// A fresh directory removed with its content at the end of the scope.
class TempDirectory
{
  public:
    TempDirectory()
        : _path(std::filesystem::temp_directory_path() / ("dup-tests-" + std::to_string(getpid()) + "-" + std::to_string(_counter++)))
    {
        std::filesystem::create_directories(_path);
    }
    ~TempDirectory()
    {
        std::error_code ignored;
        std::filesystem::remove_all(_path, ignored);
    }
    TempDirectory(const TempDirectory &) = delete;
    auto operator=(const TempDirectory &) -> TempDirectory & = delete;
    [[nodiscard]] auto GetPath() const -> const std::filesystem::path &
    {
        return _path;
    }

  private:
    static inline int _counter = 0;
    std::filesystem::path _path;
};

inline auto ReadFile(const std::filesystem::path &path) -> std::string
{
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

inline auto CountFiles(const std::filesystem::path &directory) -> int
{
    int count = 0;
    for (const auto &entry : std::filesystem::recursive_directory_iterator(directory))
    {
        count += entry.is_regular_file() ? 1 : 0;
    }
    return count;
}

/// The driver library of the tests ($DUP_PLUGIN, set by CTest); the tests that need it fail with a clear message.
inline auto PluginPath() -> std::filesystem::path
{
    const char *path = std::getenv("DUP_PLUGIN");
    REQUIRE_MESSAGE(path != nullptr, "DUP_PLUGIN is not set (run the tests through ctest or ./make.sh test-ui)");
    REQUIRE_MESSAGE(std::filesystem::exists(path), "the driver is missing, build it with ./make.sh build driver: " << path);
    return path;
}

inline auto Fixture(const std::string &name) -> std::filesystem::path
{
    const char *directory = std::getenv("DUP_FIXTURES");
    REQUIRE_MESSAGE(directory != nullptr, "DUP_FIXTURES is not set");
    const auto path = std::filesystem::path(directory) / name;
    REQUIRE_MESSAGE(std::filesystem::exists(path), "missing fixture " << path);
    return path;
}
} // namespace Dup::Tests
